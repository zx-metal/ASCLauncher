#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QStringList>
#include <QVariant>

/*!
 * Данные макросы используются для удобства и представляют собой
 * получение объекта настроек для конкретной группы
 */

class QSettings;
class ConfigManagerTest;

/*!
 * Массив вида:
 * Индекс 1 : [ значение_свойства_1, значение_свойства_2, значение_свойства_3 ]
 * Индекс 2 : [ значение_свойства_1, значение_свойства_2, значение_свойства_3 ]
 * Индекс 3 : [ значение_свойства_1, значение_свойства_2, значение_свойства_3 ]
 */
typedef QList<QList<QVariant> > ConfigArray;

/*!
 * \brief Класс, представляющий интерфейс для работы с конфиг-файлами
 */
class ConfigManager
{
    friend class ConfigManagerTest; //используется для тестов
    friend class SyncGuard;

public:
    ~ConfigManager();

    /*!
     * Возвращает указатель на объект менеджера, который настроен
     * на работу с заданной группой. Например: Database, Macros и т.д.
     * @param[in] groupName Название группы
     */
    static ConfigManager* getInstance(const QString &groupName, const QString &iniFileName);

    /*!
     * Возвращает имя группы, с которой ведется работа
     */
    QString groupName() const;

    /*!
     * Возвращает строковое значение по заданному свойству
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    QString getStringValue(const QString &propertyName, const QString &defaultValue = QString());

    /*!
     * Возвращает Double значение по заданному свойству
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    double getDoubleValue(const QString &propertyName, double defaultValue = 0);

    /*!
     * Возвращает Int значение по заданному свойству
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    int getIntegerValue(const QString &propertyName, int defaultValue = 0);

    /*!
     * Возвращает Bool значение по заданному свойству
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    bool getBoolValue(const QString &propertyName, bool defaultValue = false);

    /*!
     * Возвращает QByteArray значение по заданному свойству
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    QByteArray getByteArrayValue(const QString &propertyName, const QByteArray &defaultValue = QByteArray());

    /*!
     * Возвращает QStringList значение по заданному свойству
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    QStringList getStringListValue(const QString &propertyName, const QStringList &defaultValue = QStringList());

    /*!
     * Возвращает массив, записанный ранее в конфиг
     * @param[in] arrayName Название массива
     * @param[in] propertyNames Список свойств каждого элемента массива
     */
    ConfigArray getArray(const QString &arrayName, const QStringList &propertyNames);

    /*!
     * Возвращает значение свойства без преобразования к определенному типу
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    QVariant getVariantValue(const QString &propertyName, const QVariant &defaultValue = QVariant());

    /*!
     * Возвращает расшифрованное значение свойства
     * @param[in] propertyName Название свойства
     * @param[in] defaultValue Значение по умолчанию
     */
    QString getDecryptedStringValue(const QString &propertyName, const QString &defaultString = QString());

    /*!
     * Задает свойству значение
     * @param[in] propertyName Название свойства
     * @param[in] value Значение
     */
    void setValue(const QString &propertyName, const QVariant &value);

    /*!
     * Задает свойству строковое значение, которое будет зашифровано
     * @param[in] propertyName Название свойства
     * @param[in] text Текст
     */
    void setEncryptStringValue(const QString &propertyName, const QString &text);

    /*!
     * Задает массив значений
     * @param[in] arrayName Название массива
     * @param[in] propertyNames Список свойств каждого элемента массива
     * @param[in] array Массив значений
     */
    void setArray(const QString &arrayName, const QStringList &propertyNames, const ConfigArray &array);

    /*!
     * Возвращает true, если конфиг содержит свойство с заданным именеи
     * @param[in] propertyName Название свойства
     */
    bool containsProperty(const QString &propertyName) const;

    void reloadConfigFile();

private:
    ConfigManager(const QString &fileName = QString());

    void setGroupName(const QString &name);
    QVariant getValue(const QString &propertyName, const QVariant &defaultValue, bool encrypted = false);
    QByteArray encryptedText(const QString &text) const;

    void sync();

    //для тестов
    static ConfigManager* getTestInstance(const QString &groupName);
    QString filePath() const;

    static QMap<QString, QSharedPointer<ConfigManager>> m_instanceMap;

    QSettings *m_settings;

    quint32 m_settingsFileOwner;
    quint32 m_settingsFileOwnerGroup;

    mutable QReadWriteLock m_readWriteLock;
};

#endif // CONFIGMANAGER_H
