#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

extern "C" {
#include "miniz.h"
}

class ZipWriter
{
public:
    explicit ZipWriter(const QString& filePath);
    ~ZipWriter();

    bool isOpen() const;
    bool addFile(const QString& fileName, const QByteArray& data);
    bool close();

private:
    struct ZipArchiveDeleter
    {
        void operator()(mz_zip_archive* zip) const;
    };

    std::unique_ptr<mz_zip_archive, ZipArchiveDeleter> m_zip;
    bool m_finalized;
};
