#include "ZipReader.h"

extern "C" {
#include "miniz.h"
}

#include <QFileInfo>
#include <QDebug>
#include <cstring>
#include <limits>
#include <QFile>

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

QByteArray ZipReader::fileData(const QString& fileName)
{
    if (!m_zip) {
        qWarning() << "ZIP archive not open";
        return {};
    }

    QString normalized = fileName;
    normalized.replace('\\', '/');

    size_t size = 0;
    void* buffer = mz_zip_reader_extract_file_to_heap(
        m_zip.get(), normalized.toUtf8().constData(), &size, 0);

    if (!buffer) {
        qWarning() << "Failed to extract file:" << normalized;
        return {};
    }

    if (size == 0) {
        qWarning() << "Extracted file is empty:" << normalized;
        mz_free(buffer);
        return {};
    }

    if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
        qWarning() << "File too large for QByteArray:" << normalized << "size:" << size;
        mz_free(buffer);
        return {};
    }

    // Defensive: check for null bytes in the buffer (should not happen, but helps debug)
    if (static_cast<const char*>(buffer) == nullptr) {
        qWarning() << "Extracted buffer is null for" << normalized;
        mz_free(buffer);
        return {};
    }

    QByteArray data(static_cast<const char*>(buffer), static_cast<int>(size));
    mz_free(buffer);

    // Optional: dump extracted file for debugging (uncomment if needed)
    // QFile dumpFile("debug_extracted_" + normalized.section('/', -1));
    // if (dumpFile.open(QIODevice::WriteOnly)) dumpFile.write(data);

    qDebug() << "Successfully extracted" << normalized << "size:" << data.size();
    return data;
}



