#include "ZipReader.h"

extern "C" {
#include "miniz.h"
}

#include <QFileInfo>
#include <QDebug>
#include <cstring>

ZipReader::ZipReader(const QString &filePath) : m_zip(nullptr) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qWarning() << "ZIP file does not exist" << filePath;
        return;
    }
    m_zip = new mz_zip_archive();
    std::memset(m_zip, 0, sizeof(mz_zip_archive));
    if (!mz_zip_reader_init_file(m_zip, filePath.toUtf8().constData(), 0)) {
        qWarning() << "Failed to open ZIP" << filePath;
        delete m_zip;
        m_zip = nullptr;
    }
}

ZipReader::~ZipReader() {
    if (m_zip) {
        mz_zip_reader_end(m_zip);
        delete m_zip;
    }
}

bool ZipReader::isOpen() const {
    return m_zip != nullptr;
}

QByteArray ZipReader::fileData(const QString &fileName) {
    QByteArray result;
    if (!m_zip)
        return result;
    size_t size = 0;
    void *p = mz_zip_reader_extract_file_to_heap(m_zip, fileName.toUtf8().constData(), &size, 0);
    if (!p)
        return result;
    result = QByteArray(static_cast<const char *>(p), static_cast<int>(size));
    mz_free(p);
    return result;
}
