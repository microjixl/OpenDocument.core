#include "OfficeOpenXmlPresentationTranslator.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../StringUtil.h"
#include "../io/Storage.h"
#include "../io/StreamUtil.h"
#include "../XmlUtil.h"
#include "../crypto/CryptoUtil.h"

namespace odr {

namespace {
void XfrmTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const tinyxml2::XMLElement *offEle = in.FirstChildElement("a:off");
    if (offEle != nullptr) {
        float xIn = offEle->FindAttribute("x")->Int64Value() / 914400.0f;
        float yIn = offEle->FindAttribute("y")->Int64Value() / 914400.0f;
        out << "position:absolute;";
        out << "left:" << xIn << "in;";
        out << "top:" << yIn << "in;";
    }
    const tinyxml2::XMLElement *extEle = in.FirstChildElement("a:ext");
    if (extEle != nullptr) {
        float cxIn = extEle->FindAttribute("cx")->Int64Value() / 914400.0f;
        float cyIn = extEle->FindAttribute("cy")->Int64Value() / 914400.0f;
        out << "width:" << cxIn << "in;";
        out << "height:" << cyIn << "in;";
    }
}

void translateStyleInline(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    const tinyxml2::XMLAttribute *marLAttr = in.FindAttribute("marL");
    if (marLAttr != nullptr) {
        float marLIn = marLAttr->Int64Value() / 914400.0f;
        out << "margin-left:" << marLIn << "in;";
    }

    const tinyxml2::XMLAttribute *szAttr = in.FindAttribute("sz");
    if (szAttr != nullptr) {
        float szPt = szAttr->Int64Value() / 100.0f;
        out << "font-size:" << szPt << "pt;";
    }

    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        const std::string element = e.Name();

        if (element == "a:xfrm") XfrmTranslator(e, out, context);
    });
}
}

void OfficeOpenXmlPresentationTranslator::translateStyle(const tinyxml2::XMLElement &in, TranslationContext &context) {
}

namespace {
void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out, TranslationContext &context) {
    std::string text = in.Value();
    StringUtil::findAndReplaceAll(text, "&", "&amp;");
    StringUtil::findAndReplaceAll(text, "<", "&lt;");
    StringUtil::findAndReplaceAll(text, ">", "&gt;");

    if (!context.config->editable) {
        out << text;
    } else {
        out << R"(<span contenteditable="true" data-odr-cid=")"
            << context.currentTextTranslationIndex << "\">" << text << "</span>";
        context.textTranslation[context.currentTextTranslationIndex] = &in;
        ++context.currentTextTranslationIndex;
    }
}

void StyleAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    const std::string prefix = in.Name();

    const tinyxml2::XMLElement *pPr = in.FirstChildElement("a:pPr");
    const tinyxml2::XMLElement *rPr = in.FirstChildElement("a:rPr");
    const tinyxml2::XMLElement *spPr = in.FirstChildElement("p:spPr");
    const tinyxml2::XMLElement *endParaRPr = in.FirstChildElement("a:endParaRPr");
    if ((pPr != nullptr && pPr->FirstChild() != nullptr) ||
            (rPr != nullptr && rPr->FirstChild() != nullptr) ||
            (spPr != nullptr && spPr->FirstChild() != nullptr) ||
            (endParaRPr != nullptr && endParaRPr->FirstChild() != nullptr)) {
        out << " style=\"";
        if (pPr != nullptr) translateStyleInline(*pPr, out, context);
        if (rPr != nullptr) translateStyleInline(*rPr, out, context);
        if (spPr != nullptr) translateStyleInline(*spPr, out, context);
        if (endParaRPr != nullptr) translateStyleInline(*endParaRPr, out, context);
        out << "\"";
    }
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    StyleAttributeTranslator(in, out, context);
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);

void ParagraphTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<p";
    ElementAttributeTranslator(in, out, context);
    out << ">";

    bool empty = true;
    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e1) {
        XmlUtil::visitElementChildren(e1, [&](const tinyxml2::XMLElement &e2) {
            if (StringUtil::endsWith(e1.Name(), "Pr")) ;
            else if (std::strcmp(e1.Name(), "w:r") != 0) empty = false;
            else if (StringUtil::endsWith(e2.Name(), "Pr")) empty = false;
        });
    });

    if (empty) out << "<br/>";
    else ElementChildrenTranslator(in, out, context);

    out << "</p>";
}

void SlideTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<div class=\"slide\">";
    ElementChildrenTranslator(in, out, context);
    out << "</div>";
}

// TODO duplicated in document translation
void ImageTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<img";
    ElementAttributeTranslator(in, out, context);

    const tinyxml2::XMLElement *ref = tinyxml2::XMLHandle((tinyxml2::XMLElement &) in)
            .FirstChildElement("p:blipFill")
            .FirstChildElement("a:blip")
            .ToElement();
    if (ref == nullptr || ref->FindAttribute("r:embed") == nullptr) {
        out << " alt=\"Error: image path not specified";
        LOG(ERROR) << "image href not found";
    } else {
        const char *rIdAttr = ref->FindAttribute("r:embed")->Value();
        const Path path = Path("ppt/slides").join(context.msRelations[rIdAttr]);
        out << " alt=\"Error: image not found or unsupported: " << path << "\"";
#ifdef ODR_CRYPTO
        out << " src=\"";
        std::string image = StreamUtil::read(*context.storage->read(path));
        // hacky image/jpg working according to tom
        out << "data:image/jpg;base64, ";
        out << CryptoUtil::base64Encode(image);
        out << "\"";
#endif
    }

    out << "></img>";
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
        if (n.ToText() != nullptr) TextTranslator(*n.ToText(), out, context);
        else if (n.ToElement() != nullptr) ElementTranslator(*n.ToElement(), out, context);
    });
}

void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    static std::unordered_map<std::string, const char *> substitution{
            {"p:sp", "div"},
            {"a:r", "span"},
    };
    static std::unordered_set<std::string> skippers{
    };

    const std::string element = in.Name();
    if (skippers.find(element) != skippers.end()) return;

    if (element == "a:p") ParagraphTranslator(in, out, context);
    else if (element == "p:cSld") SlideTranslator(in, out, context);
    else if (element == "p:pic") ImageTranslator(in, out, context);
    else {
        const auto it = substitution.find(element);
        if (it != substitution.end()) {
            out << "<" << it->second;
            ElementAttributeTranslator(in, out, context);
            out << ">";
        }
        ElementChildrenTranslator(in, out, context);
        if (it != substitution.end()) {
            out << "</" << it->second << ">";
        }
    }
}
}

void OfficeOpenXmlPresentationTranslator::translateContent(const tinyxml2::XMLElement &in, TranslationContext &context) {
    ElementTranslator(in, *context.output, context);
}

}