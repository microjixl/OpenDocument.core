#ifndef ODR_OPENDOCUMENTCRYPTO_H
#define ODR_OPENDOCUMENTCRYPTO_H

#include <exception>
#include <memory>
#include <string>
#include "OpenDocumentMeta.h"

namespace odr {

class Storage;

class UnsupportedCryptoAlgorithmException : public std::exception {
public:
    const char *what() const noexcept override { return "unsupported crypto algorithm"; }
};

namespace OpenDocumentCrypto {

extern bool canDecrypt(const OpenDocumentMeta::Manifest::Entry &);
extern std::string hash(const std::string &input, OpenDocumentMeta::ChecksumType checksumType);
extern std::string decrypt(const std::string &input, const std::string &derivedKey,
        const std::string &initialisationVector, OpenDocumentMeta::AlgorithmType algorithm);
extern std::string startKey(const OpenDocumentMeta::Manifest::Entry &, const std::string &password);
extern std::string deriveKeyAndDecrypt(const OpenDocumentMeta::Manifest::Entry &, const std::string &startKey, const std::string &input);
extern bool validatePassword(const OpenDocumentMeta::Manifest::Entry &, std::string decrypted);

extern bool decrypt(std::unique_ptr<Storage> &, const OpenDocumentMeta::Manifest &, const std::string &password);

}

}

#endif //ODR_OPENDOCUMENTCRYPTO_H