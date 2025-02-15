// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AMINTERFACE_H
#define AMINTERFACE_H

#include "iteminfo.h"

#include <QObject>

#include <DDBusInterface>
#include <DFileWatcher>

DCORE_USE_NAMESPACE

using ObjectInterfaceMap = QMap<QString, QVariantMap>;
using ObjectMap = QMap<QDBusObjectPath, ObjectInterfaceMap>;
using PropMap = QMap<QString, QMap<QString, QString>>;

Q_DECLARE_METATYPE(ObjectInterfaceMap)
Q_DECLARE_METATYPE(ObjectMap)
Q_DECLARE_METATYPE(PropMap)

class ItemInfo_v3
{
    // 应用类型
    enum Categorytype {
        CategoryInternet,
        CategoryChat,
        CategoryMusic,
        CategoryVideo,
        CategoryGraphics,
        CategoryGame,
        CategoryOffice,
        CategoryReading,
        CategoryDevelopment,
        CategorySystem,
        CategoryOthers,
        CategoryErr,
    };
public:
    ItemInfo_v3();

    inline bool isInvalid() const { return m_id.isEmpty(); }

    PropMap m_actionName;
    QStringList m_actions;
    QStringList m_categories;
    bool m_autoStart;
    PropMap m_displayName;
    QString m_id;
    PropMap m_icons;
    QList<QDBusObjectPath> m_instances;
    qulonglong m_lastLaunchedTime;
    bool m_X_Flatpak;
    bool m_X_linglong;
    qulonglong m_installedTime;

    Categorytype category() const;
    friend QDebug operator<<(QDebug argument, const ItemInfo_v3 &info);
    bool operator==(const ItemInfo_v3 &other) const;
};

typedef QList<ItemInfo_v3> ItemInfoList_v3;

class ApplicationDBusProxy : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationDBusProxy(const QString &path, QObject *parent = nullptr);

signals:
    void InterfacesAdded(const QDBusObjectPath &, const ObjectInterfaceMap &);

private:
    DDBusInterface *m_applicationDBus;
};

class AppManagerDBusProxy : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerDBusProxy(QObject *parent = nullptr);

    inline QDBusPendingReply<ObjectMap> GetManagedObjects()
    {
        QList<QVariant> argumentList;
        return m_objectManagerDBus->asyncCallWithArgumentList(QStringLiteral("GetManagedObjects"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void InterfacesAdded(const QDBusObjectPath &objectPath, ObjectInterfaceMap interfacesProperties);
    void InterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);

private:
    DDBusInterface *m_objectManagerDBus;
};

class AMInter: public QObject
{
    Q_OBJECT
public:
    static AMInter *instance();

    static bool isAMReborn();
    ItemInfoList_v2 allInfos();
    QStringList allNewInstalledApps() const;
    void requestRemoveFromDesktop(const QString &appId);
    void requestSendToDesktop(const QString &appId);
    bool isOnDesktop(const QString &appId);
    void launch(const QString &desktop);
    bool isLingLong(const QString &appId) const;
    void uninstallApp(const QString &name, const bool isLinglong = false);
    void setDisableScaling(const QString &appId, const bool value);
    bool disableScaling(const QString &appId);
    QStringList autostartList() const;
    bool addAutostart(const QString &appId);
    bool removeAutostart(const QString &appId);

Q_SIGNALS: // SIGNALS
    void newAppLaunched(const QString &appId);
    void autostartChanged(const QString &action, const QString &appId);
    void itemChanged(const QString &action, ItemInfo_v2 info_v2, qlonglong categoryId);

public Q_SLOTS:
    void onInterfaceAdded(const QDBusObjectPath &object_path, const ObjectInterfaceMap &interfaces);
    void onRequestRemoveFromDesktop(QDBusPendingCallWatcher* watcher);
    void onAutoStartChanged(const bool isAutoStart, const QString &id);
    void getManagedObjectsFinished(QDBusPendingCallWatcher*);

private:
    AMInter(QObject *parent = Q_NULLPTR);

    void updateAllInfos();
    void buildConnect();
    bool llUninstall(const QString &appid);
    bool lastoreUninstall(const QString &pkgName);
    bool shouldDisableScaling(const QString &appId);
    QStringList disableScalingApps() const;
    void setDisableScalingApps(const QStringList &value);
    void monitorAutoStartFiles();
    ItemInfo_v3 itemInfoV3(const ObjectInterfaceMap &values) const;
    ItemInfo_v3 itemInfoV3(const QString &appId) const;
    ItemInfo_v2 itemInfoV2(const ItemInfo_v3 &itemInfoV3);

    AppManagerDBusProxy *m_objManagerDbusInter;
    QMap<QString, ItemInfo_v3> m_infos;
    QStringList m_autoStartApps;
    DFileWatcher *m_autoStartFileWather;
};

#endif
