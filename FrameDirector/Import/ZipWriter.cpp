#include "ZipWriter.h"

#include <QDir>
#include <QFileInfo>
#include <cstring>

namespace
{
bool ensureDirectory(const QString& path)
{
    if (path.isEmpty()) {
        return true;
    }
    QDir dir;
    return dir.mkpath(path);
}
}

void ZipWriter::ZipArchiveDeleter::operator()(mz_zip_archive* zip) const
{
    if (!zip) {
        return;
    }
    mz_zip_writer_end(zip);
    delete zip;
}

ZipWriter::ZipWriter(const QString& filePath)
    : m_zip(nullptr)
    , m_finalized(false)
{
    QFileInfo info(filePath);
    ensureDirectory(info.path());

    std::unique_ptr<mz_zip_archive, ZipArchiveDeleter> zip(new mz_zip_archive);
    std::memset(zip.get(), 0, sizeof(mz_zip_archive));

    if (!mz_zip_writer_init_file(zip.get(), filePath.toUtf8().constData(), 0)) {
        return;
    }

    m_zip = std::move(zip);
}

ZipWriter::~ZipWriter()
{
    close();
}

bool ZipWriter::isOpen() const
{
    return static_cast<bool>(m_zip);
}

bool ZipWriter::addFile(const QString& fileName, const QByteArray& data)
{
    if (!m_zip) {
        return false;
    }

    QString normalized = fileName;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));

    return mz_zip_writer_add_mem(m_zip.get(), normalized.toUtf8().constData(), data.constData(), static_cast<size_t>(data.size()), MZ_BEST_COMPRESSION) == MZ_TRUE;
}

bool ZipWriter::close()
{
    if (!m_zip) {
        return m_finalized;
    }

    if (m_finalized) {
        return true;
    }

    bool ok = mz_zip_writer_finalize_archive(m_zip.get()) == MZ_TRUE;
    mz_zip_archive* raw = m_zip.release();
    bool endOk = mz_zip_writer_end(raw) == MZ_TRUE;
    delete raw;
    m_finalized = ok && endOk;
    return m_finalized;
}
