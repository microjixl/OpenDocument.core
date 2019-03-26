#ifndef ODR_OPENDOCUMENTFILE_H
#define ODR_OPENDOCUMENTFILE_H

#include <cstdint>
#include <memory>
#include <string>
#include <map>

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

class OpenDocumentFile {
public:
    struct Entry {
        std::size_t size;
        std::size_t size_compressed;
        uint32_t index;
        std::string mediaType;
    };

    struct Meta {
        typedef int Version;
        enum class Type {
            UNKNOWN,
            TEXT,
            SPREADSHEET,
            PRESENTATION
        };
        struct Text {
            std::size_t pageCount;
        };
        struct Spreadsheet {
            struct Table {
                std::string name;
                std::size_t rowCount;
                std::size_t columnCount;
            };

            std::size_t tableCount;
            Table *tables;
        };
        struct Presentation {
            std::size_t pageCount;
        };

        Type type = Type::UNKNOWN;

        union {
            Text text;
            Spreadsheet spreadsheet;
            Presentation presentation;
        };
    };

    typedef std::map<std::string, Entry> Entries;

    static std::unique_ptr<OpenDocumentFile> create();

    virtual ~OpenDocumentFile() = default;

    virtual bool open(const std::string &) = 0;
    virtual void close() = 0;

    virtual const Entries getEntries() const = 0;
    virtual const Meta &getMeta() const = 0;
    virtual bool isFile(const std::string &) const = 0;

    virtual std::string loadText(const std::string &) = 0;
    virtual std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &) = 0;
};

}

#endif //ODR_OPENDOCUMENTFILE_H
