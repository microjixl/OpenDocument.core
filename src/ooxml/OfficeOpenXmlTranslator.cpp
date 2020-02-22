#include "OfficeOpenXmlTranslator.h"
#include <fstream>
#include <stdexcept>
#include "tinyxml2.h"
#include "../Constants.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../io/Path.h"
#include "../io/ZipStorage.h"
#include "../XmlUtil.h"
#include "OfficeOpenXmlMeta.h"
#include "OfficeOpenXmlDocumentTranslator.h"
#include "OfficeOpenXmlPresentationTranslator.h"
#include "OfficeOpenXmlWorkbookTranslator.h"

namespace odr {

class OfficeOpenXmlTranslator::Impl {
public:
    bool translate(const std::string &outPath, TranslationContext &context) const {
        std::ofstream out(outPath);
        if (!out.is_open()) return false;
        context.output = &out;

        out << Constants::getHtmlBeginToStyle();

        generateStyle(out, context);

        out << Constants::getHtmlStyleToBody();

        generateContent(context);

        out << Constants::getHtmlBodyToScript();

        generateScript(out, context);

        out << Constants::getHtmlScriptToEnd();

        out.close();
        return true;
    }

    void generateStyle(std::ofstream &out, TranslationContext &context) const {
        // default css
        out << Constants::getOpenDocumentDefaultCss();

        switch (context.meta->type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT: {
                const auto stylesXml = XmlUtil::parse(*context.storage, "word/styles.xml");
                const tinyxml2::XMLElement *styles = stylesXml->RootElement();
                OfficeOpenXmlDocumentTranslator::translateStyle(*styles, context);
            } break;
            case FileType::OFFICE_OPEN_XML_PRESENTATION: {
                // TODO duplication in generateContent
                const auto ppt = XmlUtil::parse(*context.storage, "ppt/presentation.xml");
                const tinyxml2::XMLElement *sizeEle = ppt->RootElement()->FirstChildElement("p:sldSz");
                if (sizeEle != nullptr) {
                    float widthIn = sizeEle->FindAttribute("cx")->Int64Value() / 914400.0f;
                    float heightIn = sizeEle->FindAttribute("cy")->Int64Value() / 914400.0f;

                    out << ".slide {";
                    out << "width:" << widthIn << "in;";
                    out << "height:" << heightIn << "in;";
                    out << "}";
                }
            } break;
            case FileType::OFFICE_OPEN_XML_WORKBOOK: {
                const auto stylesXml = XmlUtil::parse(*context.storage, "xl/styles.xml");
                const tinyxml2::XMLElement *styles = stylesXml->RootElement();
                OfficeOpenXmlWorkbookTranslator::translateStyle(*styles, context);
            } break;
            default:
                throw std::invalid_argument("file.getMeta().type");
        }
    }

    void generateScript(std::ofstream &of, TranslationContext &) const {
        of << Constants::getDefaultScript();
    }

    void generateContent(TranslationContext &context) const {
        switch (context.meta->type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT: {
                context.content = XmlUtil::parse(*context.storage, "word/document.xml");
                context.msRelations = OfficeOpenXmlMeta::parseRelationships(*context.storage, "word/document.xml");

                tinyxml2::XMLElement *body = context.content
                        ->FirstChildElement("w:document")
                        ->FirstChildElement("w:body");

                OfficeOpenXmlDocumentTranslator::translateContent(*body, context);
            } break;
            case FileType::OFFICE_OPEN_XML_PRESENTATION: {
                const auto ppt = XmlUtil::parse(*context.storage, "ppt/presentation.xml");
                const auto pptRelations = OfficeOpenXmlMeta::parseRelationships(*context.storage, "ppt/presentation.xml");

                XmlUtil::recursiveVisitElementsWithName(ppt->RootElement(), "p:sldId", [&](const auto &e) {
                    const std::string rId = e.FindAttribute("r:id")->Value();

                    const auto path = Path("ppt").join(pptRelations.at(rId));
                    context.content = XmlUtil::parse(*context.storage, path);
                    context.msRelations = OfficeOpenXmlMeta::parseRelationships(*context.storage, path);

                    if ((context.config->entryOffset > 0) || (context.config->entryCount > 0)) {
                        if ((context.currentEntry >= context.config->entryOffset) &&
                            (context.currentEntry < context.config->entryOffset + context.config->entryCount)) {
                            OfficeOpenXmlPresentationTranslator::translateContent(*context.content->RootElement(), context);
                        }
                    } else {
                        OfficeOpenXmlPresentationTranslator::translateContent(*context.content->RootElement(), context);
                    }

                    ++context.currentEntry;
                });
            } break;
            case FileType::OFFICE_OPEN_XML_WORKBOOK: {
                const auto xls = XmlUtil::parse(*context.storage, "xl/workbook.xml");
                const auto xlsRelations = OfficeOpenXmlMeta::parseRelationships(*context.storage, "xl/workbook.xml");

                // TODO this breaks back translation
                context.msSharedStringsDocument = XmlUtil::parse(*context.storage, "xl/sharedStrings.xml");
                XmlUtil::recursiveVisitElementsWithName(context.msSharedStringsDocument->RootElement(), "si", [&](const auto &child) {
                    context.msSharedStrings.push_back(&child);
                });

                XmlUtil::recursiveVisitElementsWithName(xls->RootElement(), "sheet", [&](const auto &e) {
                    const std::string rId = e.FindAttribute("r:id")->Value();

                    const auto path = Path("xl").join(xlsRelations.at(rId));
                    context.content = XmlUtil::parse(*context.storage, path);
                    context.msRelations = OfficeOpenXmlMeta::parseRelationships(*context.storage, path);

                    if ((context.config->entryOffset > 0) || (context.config->entryCount > 0)) {
                        if ((context.currentEntry >= context.config->entryOffset) &&
                                (context.currentEntry < context.config->entryOffset + context.config->entryCount)) {
                            OfficeOpenXmlWorkbookTranslator::translateContent(*context.content->RootElement(), context);
                        }
                    } else {
                        OfficeOpenXmlWorkbookTranslator::translateContent(*context.content->RootElement(), context);
                    }

                    ++context.currentEntry;
                });
            } break;
            default:
                throw std::invalid_argument("file.getMeta().type");
        }
    }

    bool backTranslate(const std::string &diff, const std::string &outPath, TranslationContext &context) const {
        return false;
    }
};

OfficeOpenXmlTranslator::OfficeOpenXmlTranslator() :
        impl(std::make_unique<Impl>()) {
}

OfficeOpenXmlTranslator::~OfficeOpenXmlTranslator() = default;

bool OfficeOpenXmlTranslator::translate(const std::string &outPath, TranslationContext &context) const {
    return impl->translate(outPath, context);
}

bool OfficeOpenXmlTranslator::backTranslate(const std::string &diff, const std::string &outPath, TranslationContext &context) const {
    return impl->backTranslate(diff, outPath, context);
}

}
