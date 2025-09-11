#include "ZipReader.h"

extern "C" {
#include "miniz.h"
}

#include <QFileInfo>
#include <QDebug>
#include <cstring>

// Custom deleter implementation for the unique_ptr declared in the header.
void ZipReader::ZipArchiveDeleter::operator()(mz_zip_archive *zip) const {
    if (zip) {
        mz_zip_reader_end(zip);
        delete zip;
    }
}

ZipReader::ZipReader(const QString &filePath)
{
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        qWarning() << "ZIP file does not exist" << filePath;
        return;
    }

    std::unique_ptr<mz_zip_archive, ZipArchiveDeleter> zip(new mz_zip_archive);
    std::memset(zip.get(), 0, sizeof(mz_zip_archive));

    if (!mz_zip_reader_init_file(zip.get(), filePath.toUtf8().constData(), 0)) {
        qWarning() << "Failed to open ZIP file" << filePath;
        return;
    }

    m_zip = std::move(zip);
}

ZipReader::~ZipReader() = default;

bool ZipReader::isOpen() const
{
    return static_cast<bool>(m_zip);
}

QByteArray ZipReader::fileData(const QString &fileName)
{
    QByteArray data;
    if (!m_zip)
        return data;

    // Normalise path separators for miniz.
    QString normalized = fileName;
    normalized.replace('\\', '/');

    size_t size = 0;
    void *buffer = mz_zip_reader_extract_file_to_heap(
        m_zip.get(), normalized.toUtf8().constData(), &size, 0);
    if (!buffer) {
        qWarning() << "Failed to extract" << normalized;
        return data;
    }

    // QByteArray(const char*, int) does not take ownership of the memory and may
    // in some Qt versions avoid copying. Explicitly create a QByteArray using
    // fromRawData and then copy it so that the data remains valid after we free
    // the temporary buffer returned by miniz.
    data = QByteArray(static_cast<const char*>(buffer),
        static_cast<int>(size));
    mz_free(buffer);
    return data;
}

