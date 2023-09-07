// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aminterface.h"
#include "amdbusinterface.h"
#include "appsmanager.h"
#include "util.h"

/*
 * Implementation of interface class AMInter
 */
static void registerType()
{
    qDBusRegisterMetaType<QMap<QString, QString>>();
    qDBusRegisterMetaType<ObjectInterfaceMap>();
}
Q_CONSTRUCTOR_FUNCTION(registerType)

ItemInfo_v3::ItemInfo_v3()
{
}

ItemInfo_v2 ItemInfo_v3::toItemInfo_v2()
{
    ItemInfo_v2 info_v2;

    QString defaultDisplayName;
    QString zh_displayName;
    for (auto displayNameKey : m_displayName.keys()) {
        if (displayNameKey == "Name") {
            auto displayNameValues = m_displayName.value(displayNameKey);
            for (auto key : displayNameValues.keys()) {
                if (key == "default") {
                    defaultDisplayName = displayNameValues.value(key);
                }
                if (key == "zh_CN") {
                    zh_displayName = displayNameValues.value(key);
                }
            }
        }
    }
    QString iconName;
    for (auto iconKey : m_icons.keys()) {
        if (iconKey == "default") {
            auto iconValues = m_icons.value(iconKey);
            for (auto key : iconValues.keys()) {
                if (key == "default") {
                    iconName = iconValues.value(key);
                }
            }
        }
    }
    info_v2.m_desktop = m_id;
    info_v2.m_name = zh_displayName.isEmpty() ? defaultDisplayName : zh_displayName;
    info_v2.m_key = m_id;
    info_v2.m_iconKey = iconName;
    info_v2.m_keywords.append(defaultDisplayName);
    info_v2.m_categoryId = category();

    return info_v2;
}

ItemInfo_v3::Categorytype ItemInfo_v3::category() const
{
    static QMap<QString, Categorytype> categoryTypeMap = {{"internet", CategoryInternet}, {"chat", CategoryChat}, {"music", CategoryMusic},
                         {"video", CategoryMusic}, {"graphic", CategoryGraphics}, {"office", CategoryOffice},
                         {"game", CategoryGame}, {"reading", CategoryReading}, {"development", CategoryDevelopment},
                         {"system", CategorySystem}};
    for (auto value : m_categories) {
        QString category = value.toLower();
        if (categoryTypeMap.contains(category)) {
            return categoryTypeMap.value(category);
        }
    }
    return CategoryOthers;
}

QDebug operator<<(QDebug argument, const ItemInfo_v3 &info)
{
    argument << "actionName: " << info.m_actionName << ", actions: " << info.m_actions
             << ", autoStart: " << info.m_autoStart << ", displayName: " << info.m_displayName
             << ", ID: " << info.m_id << ", icons: " << info.m_icons
             << ", instances: " << info.m_instances << ", lastLaunchedTime: " << info.m_lastLaunchedTime
             << ", categories: " << info.m_categories
             << ", X_Flatpak: " << info.m_X_Flatpak << ", X_linglong: " << info.m_X_linglong
             << ", installedTime: " << info.m_installedTime;

    return argument;
}

bool ItemInfo_v3::operator==(const ItemInfo_v3 &other) const
{
    return (this->m_actionName == other.m_actionName) &&
           (this->m_actions == other.m_actions) &&
           (this->m_autoStart == other.m_autoStart) &&
           (this->m_displayName == other.m_displayName) &&
           (this->m_id == other.m_id) &&
           (this->m_icons == other.m_icons) &&
           (this->m_instances == other.m_instances) &&
           (this->m_lastLaunchedTime == other.m_lastLaunchedTime) &&
           (this->m_categories == other.m_categories) &&
           (this->m_X_Flatpak == other.m_X_Flatpak) &&
           (this->m_X_linglong == other.m_X_linglong) &&
           (this->m_installedTime == other.m_installedTime);
}

static const QString keyAppsDisableScaling = "Apps_Disable_Scaling";

AMInter::AMInter(QObject *parent)
    : QObject(parent)
    , m_amDbusInter(new AMDBusInter(this))
{
    getAllInfos();
    buildConnect();
    monitorAutoStartFiles();
    connect(m_amDbusInter, &AMDBusInter::InterfacesAdded, this, [this] (const QDBusObjectPath &objectPath, ObjectInterfaceMap objs){
        qInfo() << objectPath.path() << objs;
        ItemInfo_v3 info_v3 = getItemInfo_v3(objs);
        m_info.insert(objectPath.path(), info_v3);
        m_itemInfoList_v3.append(info_v3);
        ItemInfo_v2 info_v2 = info_v3.toItemInfo_v2();
        Q_EMIT itemChanged("created", info_v2, info_v3.category());
    });
    connect(m_amDbusInter, &AMDBusInter::InterfacesRemoved, this, [] (const QDBusObjectPath &object_path, const QStringList &interface){
        qInfo() << object_path.path() << interface;
        // TODO: util AM reborn finish the function!
    });
}

AMInter *AMInter::instance()
{
    static AMInter AMInter;
    return &AMInter;
}

bool AMInter::isAMReborn()
{
    static bool hasEnv = false;
    if (!hasEnv) {
        hasEnv = true;
        if (qEnvironmentVariableIsSet("DISABLE_AM_REBORN"))
            return !(qEnvironmentVariable("DISABLE_AM_REBORN") == "1");
    }

#ifdef AM_REBORN
    return true;
#else
    return false
#endif
}

ItemInfoList_v2 AMInter::getAllItemInfos()
{
    for (auto info : m_itemInfoList_v3) {
        m_itemInfoList_v2.append(info.toItemInfo_v2());
    }
    return m_itemInfoList_v2;
}

QStringList AMInter::getAllNewInstalledApps()
{
    QStringList apps;
    for (auto info : m_itemInfoList_v3) {
        if (info.m_lastLaunchedTime == 0) {
            apps.append(info.m_id);
        }
    }
    return apps;
}

void AMInter::requestRemoveFromDesktop(const QString &appId)
{
    const auto itemInfoList_v3 = m_info.values();
    auto it = std::find_if(itemInfoList_v3.begin(), itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == itemInfoList_v3.end())
        return;
    QDBusInterface iface(AM_SERVICE_NAME, m_info.key(*it), APP_INTERFACE_NAME, QDBusConnection::sessionBus(), this);
    if (iface.isValid()) {
        QDBusReply<bool> reply = iface.call("RemoveFromDesktop");
        if (reply.isValid()) {
            qDebug() << "RemoveFromDesktop:" << appId << reply.value();
        } else {
            qWarning() << "RemoveFromDesktop failed!";
        }
    } else {
        qWarning() << "RemoveFromDesktop" << qPrintable(QDBusConnection::sessionBus().lastError().message());
    }
}

void AMInter::requestSendToDesktop(const QString &appId)
{
    const auto itemInfoList_v3 = m_info.values();
    auto it = std::find_if(itemInfoList_v3.begin(), itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == itemInfoList_v3.end())
        return;
    QDBusInterface iface(AM_SERVICE_NAME, m_info.key(*it), APP_INTERFACE_NAME, QDBusConnection::sessionBus(), this);
    if (iface.isValid()) {
        QDBusReply<bool> reply = iface.call("SendToDesktop");
        if (reply.isValid()) {
            qDebug() << "SendToDesktop:" << appId << reply.value();
        } else {
            qWarning() << "SendToDesktop failed!" << qPrintable(QDBusConnection::sessionBus().lastError().message());
        }
    } else {
        qWarning() << "SendToDesktop" << qPrintable(QDBusConnection::sessionBus().lastError().message());
    }
}

void AMInter::getAllInfos()
{
    ObjectMap infos = m_amDbusInter->GetManagedObjects();
    for (auto iter = infos.begin(); iter != infos.end(); ++iter) {
        const auto &objPath = iter.key();
        const auto &objs = iter.value();
        auto info_v3 = getItemInfo_v3(objs);
//        info_v3.m_objectPath = objPath.path();
        m_info.insert(objPath.path(), info_v3);
        m_itemInfoList_v3.append(info_v3);
        if (info_v3.m_autoStart) {
            m_autoStartApps.append(info_v3.m_id);
        }
    }
}

void AMInter::buildConnect()
{
    for (auto info : m_info.keys()) {
        DBusProxy *autoStartInter = new DBusProxy(AM_SERVICE_NAME, info, AM_INTERFACE_NAME, this);
        connect(autoStartInter, &DBusProxy::InterfacesAdded, this, &AMInter::onInterfaceAdded);
    }
}

bool AMInter::isOnDesktop(QString appId)
{
    const auto itemInfoList_v3 = m_info.values();
    auto it = std::find_if(itemInfoList_v3.begin(), itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == itemInfoList_v3.end())
        return false;
    QDBusInterface iface(AM_SERVICE_NAME, m_info.key(*it), APP_INTERFACE_NAME, QDBusConnection::sessionBus(), this);
    if (iface.isValid()) {
        QVariant value = iface.property("isOnDesktop");
        return value.toBool();
    } else {
        qWarning() << "isOnDesktop" << qPrintable(QDBusConnection::sessionBus().lastError().message());
    }
    return false;
}

void AMInter::launch(const QString &desktop)
{
    const auto itemInfoList_v3 = m_info.values();
    auto it = std::find_if(itemInfoList_v3.begin(), itemInfoList_v3.end(), [desktop](ItemInfo_v3 info){
        return info.m_id == desktop;
    });
    if (it == itemInfoList_v3.end())
        return;
    QDBusInterface iface(AM_SERVICE_NAME, m_info.key(*it), APP_INTERFACE_NAME, QDBusConnection::sessionBus(), this);
    if (iface.isValid()) {
        QVariantList arguments;
        arguments << QString();
        arguments << QStringList();
        QMap<QString, QVariant> arg3;
        if (!it->m_id.isEmpty() && !shouldDisableScaling(it->m_id)) {
            auto dbus = QDBusConnection::sessionBus();
            QDBusMessage reply = dbus.call(QDBusMessage::createMethodCall("org.deepin.dde.XSettings1",
                                                                          "/org/deepin/dde/XSettings1",
                                                                          "org.deepin.dde.XSettings1",
                                                                          "GetScaleFactor"), QDBus::Block, 2);
            QString env;
            if (reply.type() == QDBusMessage::ReplyMessage) {
                QDBusReply<double> ret(reply);
                double scale = ret.isValid() ? ret.value() : 1.0;
                scale = scale > 0 ? scale : 1;
                const QString scaleStr = QString::number(scale, 'f', -1);
                env= QString("DEEPIN_WINE_SCALE=%1").arg(scaleStr);
                arg3.insert("env", QVariant::fromValue(env));
            }
        }
        arguments << QVariant::fromValue(arg3);
        QDBusReply<QDBusObjectPath> reply = iface.callWithArgumentList(QDBus::Block, "Launch", arguments);
        if (reply.isValid()) {
            qDebug() << "Launch:" << desktop << reply.value().path();
        } else {
            qWarning() << "Launch failed!" << qPrintable(QDBusConnection::sessionBus().lastError().message());
        }
    } else {
        qWarning() << "Launch" << qPrintable(QDBusConnection::sessionBus().lastError().message());
    }
}

bool AMInter::isLingLong(const QString &appId) const
{
    auto it = std::find_if(m_itemInfoList_v3.begin(), m_itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it != m_itemInfoList_v3.end()) {
        return it->m_X_linglong;
    }
    return false;
}

void AMInter::onInterfaceAdded(const QDBusObjectPath &objPath, const ObjectInterfaceMap &interfaces)
{
    QString path = objPath.path();
    auto index = path.lastIndexOf("/");
    path.truncate(index);
    const QStringList objPaths = m_info.keys();
    if (!objPaths.contains(path))
        return;

    Q_EMIT newAppLaunched(m_info.value(path).m_id);
}

void AMInter::onRequestRemoveFromDesktop(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QString> reply = *watcher;
    if (!reply.isError()) {
        QString name= reply.argumentAt<0>();
        qDebug()<<"QDBusPendingReply name = "<<name;
    }
    watcher->deleteLater();
}

void AMInter::onAutoStartChanged(const bool isAutoStart, const QString& id)
{
    if (isAutoStart) {
        Q_EMIT autostartChanged("added", id);
    } else {
        Q_EMIT autostartChanged("deleted", id);
    }
}

void AMInter::uninstallApp(const QString &name, bool isLinglong)
{
    bool ret = false;
    if (isLinglong) {
        ret = ll_uninstall(name);
    } else {
        ret = lastore_uninstall(name);
    }

    if (ret) {
        AppsManager::instance()->abandonStashedItem(name);
    } else {
        AppsManager::instance()->onUninstallFail(name);
    }

    // 刷新各列表的分页信息
    emit AppsManager::instance()->dataChanged(AppsListModel::FullscreenAll);
}

void AMInter::setDisableScaling(QString appId, bool value)
{
    auto it = std::find_if(m_itemInfoList_v3.begin(), m_itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == m_itemInfoList_v3.end())
        return;
    QStringList apps = getDisableScalingApps();
    if (value) {
        if (apps.contains(appId)) {
            return;
        }
        apps.append(appId);
    } else {
        bool found = false;
        for (auto iter = apps.begin(); iter != apps.end(); iter++) {
            if (*iter == appId) {
                found = true;
                apps.erase(iter);
                break;
            }
        }
        if (!found)
            return;
    }

    setDisableScalingApps(apps);
}

bool AMInter::getDisableScaling(QString appId)
{
    auto it = std::find_if(m_itemInfoList_v3.begin(), m_itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == m_itemInfoList_v3.end())
        return false;

    if (getDisableScalingApps().contains(appId))
        return true;

    return false;
}

QStringList AMInter::autostartList() const
{
    return m_autoStartApps;
}

bool AMInter::addAutostart(const QString &appId)
{
    const auto itemInfoList_v3 = m_info.values();
    auto it = std::find_if(itemInfoList_v3.begin(), itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == itemInfoList_v3.end())
        return false;

    QDBusInterface iface(AM_SERVICE_NAME, m_info.key(*it), APP_INTERFACE_NAME, QDBusConnection::sessionBus(), this);
    if (iface.isValid()) {
        bool ret = iface.setProperty("AutoStart", true);
        return ret;
    } else {
        qWarning() << "AutoStart" << qPrintable(QDBusConnection::sessionBus().lastError().message());
        return false;
    }
}

bool AMInter::removeAutostart(const QString &appId)
{
    const auto itemInfoList_v3 = m_info.values();
    auto it = std::find_if(itemInfoList_v3.begin(), itemInfoList_v3.end(), [appId](ItemInfo_v3 info){
        return info.m_id == appId;
    });
    if (it == itemInfoList_v3.end())
        return false;

    QDBusInterface iface(AM_SERVICE_NAME, m_info.key(*it), APP_INTERFACE_NAME, QDBusConnection::sessionBus(), this);
    if (iface.isValid()) {
        bool ret = iface.setProperty("AutoStart", false);
        return ret;
    } else {
        qWarning() << "AutoStart" << qPrintable(QDBusConnection::sessionBus().lastError().message());
        return false;
    }
}

bool AMInter::ll_uninstall(const QString &appid)
{
    QProcess ll;
    ll.setProgram(QStandardPaths::findExecutable("ll-cli"));
    ll.setArguments(QStringList() << "uninstall" << appid);
    ll.start();
    ll.waitForFinished();

    if (ll.exitCode() != 0) {
        qWarning() << "uninstall " << appid << "failed. " << ll.errorString();
        return false;
    }

    return true;
}

bool AMInter::lastore_uninstall(const QString &pkgName)
{
    QDBusInterface lastoreDbus("org.deepin.dde.Lastore1",
                               "/org/deepin/dde/Lastore1",
                               "org.deepin.dde.Lastore1.Manager",
                               QDBusConnection::systemBus());

    QDBusReply<bool> reply = lastoreDbus.call("PackageExists", pkgName);
    // 包未安装时
    if (!(reply.isValid() && reply.value())) {
        qWarning() << "check packget" << pkgName << " exists failed" << reply.error();
        return false;
    }

    // RemovePackage 接口有白名单，需要加上 /usr/bin/dde-launcher
    QDBusReply<QDBusObjectPath> rmReply = lastoreDbus.call(QDBus::Block,
                                                           "RemovePackage",
                                                           QString("%1-rm-job").arg(pkgName), // jobname ?
                                                           pkgName);

    if (!rmReply.isValid() || rmReply.value().path().isEmpty()) {
        qWarning() << "RemovePackage failed: " << rmReply.error();
        return false;
    }

    return true;
}

bool AMInter::shouldDisableScaling(QString appId)
{
    auto disableScalingApps = getDisableScalingApps();
    return disableScalingApps.contains(appId);
}

QStringList AMInter::getDisableScalingApps()
{
    QStringList ret;

    QVariant value = ConfigWorker::getValue(keyAppsDisableScaling, ret);
    auto apps = value.toList();
    for (auto app : apps) {
        ret.append(app.toString());
    }

    return ret;
}

void AMInter::setDisableScalingApps(const QStringList &value)
{
    QList<QVariant> apps;
    for (const auto &app : value)
        apps.push_back(app);

    ConfigWorker::setValue(keyAppsDisableScaling, apps);
}

void AMInter::monitorAutoStartFiles()
{
    m_autoStartFileWather = new DFileWatcher( QDir::homePath() + "/.config/autostart");
    connect(m_autoStartFileWather, &DBaseFileWatcher::subfileCreated, this, [this](const QUrl &url){
        QString desktop = url.toString().split('/').last();
        desktop.truncate(desktop.lastIndexOf('.'));
        Q_EMIT autostartChanged("added", desktop);
        qWarning() << "autostartChanged added" << desktop;
    });
    connect(m_autoStartFileWather, &DBaseFileWatcher::fileDeleted, this, [this](const QUrl &url){
        QString desktop = url.toString().split('/').last();
        desktop.truncate(desktop.lastIndexOf('.'));
        Q_EMIT autostartChanged("deleted", desktop);
        qWarning() << "autostartChanged deleted" << desktop;
    });
    bool ret = m_autoStartFileWather->startWatcher();
    if (!ret) {
        qWarning() << "autoStartFileWather failed!";
    }
}

ItemInfo_v3 AMInter::getItemInfo_v3(const ObjectInterfaceMap &objs)
{
    ItemInfo_v3 info_v3;
    for (auto objIter = objs.begin(); objIter != objs.end(); ++objIter) {
        const auto obj = objIter.key();
        if (obj != "org.desktopspec.ApplicationManager1.Application")
            continue;
        const auto interProps = objIter.value();
        for (auto interPropIter = interProps.begin(); interPropIter != interProps.end(); ++interPropIter) {
            const auto key = interPropIter.key();
            if (key == "ActionName") {
                PropMap value = qdbus_cast<PropMap>(interPropIter.value());
                info_v3.m_actionName = value;
            } else if (key == "Actions") {
                QStringList value = qdbus_cast<QStringList>(interPropIter.value());
                info_v3.m_actions = value;
            } else if (key == "AutoStart") {
                bool value = qdbus_cast<bool>(interPropIter.value());
                info_v3.m_autoStart = value;
            } else if (key == "Categories") {
                QStringList value = qdbus_cast<QStringList>(interPropIter.value());
                info_v3.m_categories = value;
            } else if (key == "DisplayName") {
                PropMap value = qdbus_cast<PropMap>(interPropIter.value());
                info_v3.m_displayName = value;
            } else if (key == "ID") {
                QString value = qdbus_cast<QString>(interPropIter.value());
                info_v3.m_id = value;
            } else if (key == "Icons") {
                PropMap value = qdbus_cast<PropMap>(interPropIter.value());
                info_v3.m_icons = value;
            } else if (key == "Instances") {
                QList<QDBusObjectPath> value = qdbus_cast<QList<QDBusObjectPath>>(interPropIter.value());
                info_v3.m_instances = value;
            } else if (key == "LastLaunchedTime") {
                qulonglong value = qdbus_cast<qulonglong>(interPropIter.value());
                info_v3.m_lastLaunchedTime = value;
            } else if (key == "X_Flatpak") {
                bool value = qdbus_cast<bool>(interPropIter.value());
                info_v3.m_X_Flatpak = value;
            } else if (key == "X_linglong") {
                bool value = qdbus_cast<bool>(interPropIter.value());
                info_v3.m_X_linglong = value;
            } else if (key == "installedTime") {
                qulonglong value = qdbus_cast<qulonglong>(interPropIter.value());
                info_v3.m_installedTime = value;
            } else if (key == "AutoStart") {
                qulonglong value = qdbus_cast<qulonglong>(interPropIter.value());
                info_v3.m_installedTime = value;
            }
        }
    }
    return info_v3;
}

DBusProxy::DBusProxy(const QString &service, const QString &path, const QString &interface, QObject *parent)
    : QObject(parent)
    , m_dBusDisplayInter(new DDBusInterface(service, path, interface, QDBusConnection::sessionBus(), this))
{
}

bool DBusProxy::autoStart() const
{
    return m_dBusDisplayInter->property("AutoStart").toBool();
}
