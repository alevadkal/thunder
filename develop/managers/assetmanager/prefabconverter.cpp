#include "prefabconverter.h"

#include "components/actor.h"

#include <QFile>
#include <QDebug>

#include <bson.h>
#include <json.h>

#include <editor/pluginmanager.h>
#include <editor/projectmanager.h>

#define FORMAT_VERSION 2

PrefabConverterSettings::PrefabConverterSettings() {
    setType(MetaType::type<Prefab *>());
    setVersion(FORMAT_VERSION);
}

QStringList PrefabConverterSettings::typeNames() const {
    return { "Prefab" };
}

bool PrefabConverterSettings::isReadOnly() const {
    return false;
}

QString PrefabConverterSettings::defaultIcon(QString) const {
    return ":/Style/styles/dark/images/prefab.svg";
}

AssetConverterSettings *PrefabConverter::createSettings() const {
    return new PrefabConverterSettings();
}

QString PrefabConverter::templatePath() const {
    return ":/Templates/Prefab.fab";
}

Actor *PrefabConverter::createActor(const AssetConverterSettings *settings, const QString &guid) const {
    PROFILE_FUNCTION();

    Prefab *prefab = Engine::loadResource<Prefab>(guid.toStdString());
    if(prefab) {
        return static_cast<Actor *>(prefab->actor()->clone());
    }
    return AssetConverter::createActor(settings, guid);
}

AssetConverter::ReturnCode PrefabConverter::convertFile(AssetConverterSettings *settings) {
    PROFILE_FUNCTION();

    QFile src(settings->source());
    if(src.open(QIODevice::ReadOnly)) {
        string data = src.readAll().toStdString();
        src.close();

        Variant actor = readJson(data, settings);
        injectResource(actor, requestResource());

        QFile file(settings->absoluteDestination());
        if(file.open(QIODevice::WriteOnly)) {
            ByteArray data = Bson::save(actor);
            file.write((const char *)&data[0], data.size());
            file.close();

            return Success;
        }
    }
    return InternalError;
}

Variant PrefabConverter::readJson(const string &data, AssetConverterSettings *settings) {
    PROFILE_FUNCTION();

    Variant result = Json::load(data);

    bool update = false;
    switch(settings->currentVersion()) {
        case 0: update |= toVersion1(result);
        case 1: update |= toVersion2(result);
        case 2: update |= toVersion3(result);
        case 3: update |= toVersion4(result);
        default: break;
    }

    if(update) {
        QFile src(settings->source());
        if(src.open(QIODevice::WriteOnly)) {
            settings->setCurrentVersion(settings->version());

            string data = Json::save(result, 0);
            src.write(data.c_str(), data.size());
            src.close();
        }
    }

    return result;
}

void PrefabConverter::injectResource(Variant &origin, Resource *resource) {
    PROFILE_FUNCTION();

    VariantList &objects = *(reinterpret_cast<VariantList *>(origin.data()));
    VariantList &o = *(reinterpret_cast<VariantList *>(objects.front().data()));

    auto i = o.begin(); // type
    i++; // uuid
    i++; // parent
    *i = resource->uuid();

    objects.push_front(Engine::toVariant(resource).toList().front());

    QSet<QString> modules;
    for(auto &it : objects) {
        VariantList *object = reinterpret_cast<VariantList *>(it.data());
        if(object) {
            QString type = QString::fromStdString(object->begin()->toString());
            QString module = PluginManager::instance()->getModuleName(type);
            if(!module.isEmpty() && module != (QString("Module") + ProjectManager::instance()->projectName())) {
                modules.insert(module);
            }
        }
    }
    ProjectManager::instance()->reportModules(modules);

    Engine::unloadResource(resource);
}

Resource *PrefabConverter::requestResource() {
    return Engine::objectCreate<Prefab>();
}

bool PrefabConverter::toVersion1(Variant &variant) {
    PROFILE_FUNCTION();

    // Create all declared objects
    VariantList &objects = *(reinterpret_cast<VariantList *>(variant.data()));
    for(auto &it : objects) {
        VariantList &o = *(reinterpret_cast<VariantList *>(it.data()));
        if(o.size() >= 5) {
            auto i = o.begin();
            ++i;
            ++i;
            ++i;
            ++i;

            // Load base properties
            VariantMap &properties = *(reinterpret_cast<VariantMap *>((*i).data()));
            VariantMap propertiesNew;
            for(auto &prop : properties) {
                QString property(prop.first.c_str());

                property.replace("_Rotation", "quaternion");
                property.replace("Use_Kerning", "kerning");
                property.replace("Audio_Clip", "clip");
                property.replace('_', "");
                property.replace(0, 1, property[0].toLower());

                propertiesNew[property.toStdString()] = prop.second;
            }
            properties = propertiesNew;
        }
    }
    return true;
}

bool PrefabConverter::toVersion2(Variant &variant) {
    return false;
}

bool PrefabConverter::toVersion3(Variant &variant) {
    return false;
}

bool PrefabConverter::toVersion4(Variant &variant) {
    return false;
}
