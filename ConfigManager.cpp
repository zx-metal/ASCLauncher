#include "ConfigManager.h"

#include <QSettings>
#include <QStringList>
#include <QTextCodec>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QMutexLocker>

#include <unistd.h>

namespace  {
    QMutex mutex;
}

QMap<QString, QSharedPointer<ConfigManager>> ConfigManager::m_instanceMap;

class SyncGuard
{
public:
    explicit SyncGuard(ConfigManager *config)
        : m_config(config)
    {
    }

    ~SyncGuard()
    {
        m_config->sync();
    }

private:
    ConfigManager *m_config = nullptr;
};

ConfigManager::~ConfigManager()
{
    delete m_settings;
}

ConfigManager *ConfigManager::getInstance(const QString &groupName, const QString &iniFileName)
{
    QMutexLocker locker(&mutex);
    Q_UNUSED(locker);

    if (!m_instanceMap.contains(groupName)) {
        ConfigManager *m = new ConfigManager(iniFileName);
        m->setGroupName(groupName);
        m_instanceMap[groupName].reset(m);
        return m;
    }

    return m_instanceMap.value(groupName).data();
}

QString ConfigManager::groupName() const
{
    return m_settings->group();
}

QString ConfigManager::getStringValue(const QString &propertyName, const QString &defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue).toString();
}

double ConfigManager::getDoubleValue(const QString &propertyName, double defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue).toDouble();
}

int ConfigManager::getIntegerValue(const QString &propertyName, int defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue).toInt();
}

bool ConfigManager::getBoolValue(const QString &propertyName, bool defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue).toBool();
}

QByteArray ConfigManager::getByteArrayValue(const QString &propertyName, const QByteArray &defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue).toByteArray();
}

QStringList ConfigManager::getStringListValue(const QString &propertyName, const QStringList &defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue).toStringList();
}

ConfigArray ConfigManager::getArray(const QString &arrayName, const QStringList &propertyNames)
{
    QReadLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    ConfigArray array;

    int size = m_settings->beginReadArray(arrayName);
    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        QVariantList list;

        foreach (QString name, propertyNames) {
            list << m_settings->value(name);
        }

        array.append(list);
    }
    m_settings->endArray();

    return array;
}

QVariant ConfigManager::getVariantValue(const QString &propertyName, const QVariant &defaultValue)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return getValue(propertyName, defaultValue);
}

QString ConfigManager::getDecryptedStringValue(const QString &propertyName, const QString &defaultString)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    QByteArray array = QByteArray::fromHex(getValue(propertyName, defaultString, true).toByteArray());

    if (!array.isEmpty()) {
        int magicWordSize = QByteArray("skpt").toBase64().size();
        array = array.left(array.size() - magicWordSize);
        array = QByteArray::fromBase64(array);

        QString str(array);
        if (str.size() > 1) {
            QChar firstChar = str[0];
            str[0] = str.at(str.size() - 1);
            str[str.size() - 1] = firstChar;
        }

        return str;
    }

    return defaultString;
}

void ConfigManager::setValue(const QString &propertyName, const QVariant &value)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    if (propertyName.isEmpty())
        return;

    SyncGuard guard(this); Q_UNUSED(guard);
    m_settings->setValue(propertyName, value);
}

void ConfigManager::setEncryptStringValue(const QString &propertyName, const QString &text)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    if (propertyName.isEmpty())
        return;

    SyncGuard guard(this); Q_UNUSED(guard);
    m_settings->setValue(propertyName, QString(encryptedText(text)));
}

void ConfigManager::setArray(const QString &arrayName, const QStringList &propertyNames, const ConfigArray &array)
{
    QWriteLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    if (arrayName.isEmpty()|| !propertyNames.count())
        return;

    SyncGuard guard(this); Q_UNUSED(guard);

    //удаляем из настроек, если передан пустой массив
    if (array.isEmpty()) {
        m_settings->beginGroup(arrayName);
        m_settings->remove("");
        m_settings->endGroup();
    }

    for (int i = 0; i < array.count(); ++i) {
        //если массив значений не совпадает с количеством свойств, тогда сохранение прерывается
        if (array.at(i).count() != propertyNames.count())
            return;
    }

    m_settings->beginWriteArray(arrayName);
    for (int i = 0; i < array.count(); ++i) {
        m_settings->setArrayIndex(i);

        for (int j = 0; j < propertyNames.count(); ++j) {
            m_settings->setValue(propertyNames.at(j), array.at(i).at(j));
        }
    }
    m_settings->endArray();
}

bool ConfigManager::containsProperty(const QString &propertyName) const
{
    QReadLocker locker(&m_readWriteLock);
    Q_UNUSED(locker);

    return m_settings->contains(propertyName);
}

void ConfigManager::reloadConfigFile()
{
    QReadLocker locker(&m_readWriteLock);
    Q_UNUSED(locker)

    SyncGuard guard(this);
    Q_UNUSED(guard);
}

ConfigManager::ConfigManager(const QString &fileName)
    : m_settingsFileOwner(0)
    , m_settingsFileOwnerGroup(0)
{
    QDir dir(qApp->applicationDirPath());
    // dir.cdUp();
    // dir.cd("data");

    QString iniFilePath = dir.absolutePath() + "/" + (fileName.isEmpty() ? "skpt.ini" : fileName);
    QFileInfo fileInfo(iniFilePath);
    if (fileInfo.exists()) {
        m_settingsFileOwner = fileInfo.ownerId();
        m_settingsFileOwnerGroup = fileInfo.groupId();
    }
    m_settings = new QSettings(iniFilePath, QSettings::IniFormat);
    qDebug() << m_settings->fileName();
    m_settings->setIniCodec(QTextCodec::codecForName("UTF8"));
}

void ConfigManager::setGroupName(const QString &name)
{
    if (m_settings->group() != name) {
        if (!m_settings->group().isEmpty())
            m_settings->endGroup();

        m_settings->beginGroup(name);
    }
}

QVariant ConfigManager::getValue(const QString &propertyName, const QVariant &defaultValue, bool encrypted)
{   
    if (!m_settings->allKeys().contains(propertyName) || (m_settings->value(propertyName).toByteArray().isEmpty() && m_settings->value(propertyName).toStringList().isEmpty())) {
        SyncGuard guard(this); Q_UNUSED(guard);
        if (encrypted)
            m_settings->setValue(propertyName, QString(encryptedText(defaultValue.toString())));
        else
            m_settings->setValue(propertyName, defaultValue);
    }
    return m_settings->value(propertyName, defaultValue);
}

QByteArray ConfigManager::encryptedText(const QString &text) const
{
    QString modifiedText = text;
    if (modifiedText.size() > 1) {
        QChar firstChar = modifiedText[0];
        modifiedText[0] = modifiedText.at(modifiedText.size() - 1);
        modifiedText[modifiedText.size() - 1] = firstChar;
    }

    QByteArray encryptedArray;
    encryptedArray.append(modifiedText);
    encryptedArray = (encryptedArray.toBase64() + QByteArray("skpt").toBase64()).toHex();

    return encryptedArray;
}

void ConfigManager::sync()
{
    m_settings->sync();
    unsigned int currentUserId = getuid();
    if (currentUserId == 0) { //root user
        int returnCode = chown(m_settings->fileName().toLocal8Bit().data(), m_settingsFileOwner, m_settingsFileOwnerGroup);
        if (returnCode != 0) {
            qCritical() << "chown returned non-zero code:" << errno;
        }
    }
}

ConfigManager *ConfigManager::getTestInstance(const QString &groupName)
{
    ConfigManager *testInstance = new ConfigManager("test_skpt.ini");

    testInstance->setGroupName(groupName);

    return testInstance;
}

QString ConfigManager::filePath() const
{
    return m_settings->fileName();
}
