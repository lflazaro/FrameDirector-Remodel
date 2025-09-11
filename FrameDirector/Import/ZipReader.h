#pragma once

#include <QString>
#include <QByteArray>

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
    struct mz_zip_archive *m_zip;
};
