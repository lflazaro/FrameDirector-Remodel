#pragma once

#include <QString>
#include <QByteArray>
#include <memory>

extern "C" {
#include "miniz.h"
}

// Simple ZIP reader based on the miniz library.
// Provides minimal functionality needed by ORAImporter.
class ZipReader {
public:
    explicit ZipReader(const QString &filePath);
    ~ZipReader();

    // Returns true if the archive was opened successfully.
    bool isOpen() const;

    // Extracts the file at the given path inside the archive and returns its
    // contents. Returns an empty QByteArray on failure.
    QByteArray fileData(const QString &fileName);

private:
    struct ZipArchiveDeleter {
        void operator()(mz_zip_archive *zip) const;
    };
    std::unique_ptr<mz_zip_archive, ZipArchiveDeleter> m_zip;
};
