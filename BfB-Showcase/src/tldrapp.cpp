/* Copyright (c) 2012 Research In Motion Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tldrapp.h"
#include "netimagemanager.h"
#include "netimagetracker.h"
#include "bbm/BBMHandler.hpp"

#include <bb/cascades/AbstractPane>
#include <bb/cascades/LocaleHandler>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/Container>
#include <bb/cascades/SceneCover>
#include <bb/data/DataSource>
#include <bb/system/SystemDialog>

#include "ActiveFrameQML.h"

using namespace bb::cascades;
using namespace bb::data;
using namespace bb::system;

TLDRApp::TLDRApp(bb::cascades::Application *app) :
         QObject(app)
{
    DataSource::registerQmlTypes();

    // Prepare localization.Connect to the LocaleHandlers systemLanguaged change signal, this will
    // tell the application when it is time to load a new set of language strings.
    mTranslator = new QTranslator(this);
    mLocaleHandler = new LocaleHandler(this);
    connect(mLocaleHandler, SIGNAL(systemLanguageChanged()),
            SLOT(onSystemLanguageChanged()));
    onSystemLanguageChanged();


    //Kick off BBM Registration.
    //TODO: Define your own UUID here. You can generate one here: http://www.guidgenerator.com/
	const QString uuid(QLatin1String("fd34de4e-8671-442a-8c42-8c4743463400"));
	BBMHandler *bbmHandler = new BBMHandler(uuid, app);
	bbmHandler->registerApplication();


    // Register all our types and the system dialog
    qmlRegisterType<NetImageTracker>("com.netimage", 1, 0, "NetImageTracker");
    qmlRegisterType<NetImageManager>("com.netimage", 1, 0, "NetImageManager");
    qmlRegisterType<SystemDialog>("my.systemDialogs", 1, 0, "SystemDialog");

    // Create scene document from main.qml asset
    // set parent to created document to ensure it exists for the whole application lifetime
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);
    qml->setContextProperty("tldrApp", this);

    //Expose the BBM Registration handler to main.qml.
    qml->setContextProperty("bbmHandler", bbmHandler);

    // Create Active Frame (shown when app is miminized)
    ActiveFrameQML *activeFrame = new ActiveFrameQML();
    Application::instance()->setCover(activeFrame);
    activeFrame->setContextProperty("tldrApp", this);

    // create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();

    // set created root object as a scene
    app->setScene(root);

}

TLDRApp::~TLDRApp()
{
}

QString TLDRApp::findImage(const QVariant item)
{
    // The item variable is the xml data from rss-feed
    QVariantMap itemData = item.toMap();

    // Some streams have the image data in a media:content container, of so let's get it
    if (itemData.contains("media:content")) {
        QVariantMap itemMediaMap;

        if (itemData["media:content"].canConvert<QVariantList>()) {
            QVariantList itemMediaList = itemData["media:content"].toList();

            if (itemMediaList.size() > 0) {
                itemMediaMap = itemMediaList.at(1).toMap();
            } else {
                itemMediaMap = itemMediaList.at(0).toMap();
            }

        } else if (itemData["media:content"].canConvert<QVariantMap>()) {
            itemMediaMap = itemData["media:content"].toMap();

        } else {
            // Well, there was some new way we have not encountered before
            qDebug() << "could not convert " << itemData["media:content"];
        }
        // Sometimes it's an URL
        if (itemMediaMap.contains("url")) {
            return itemMediaMap["url"].toString();
        }
    // If the image data is not in media:content it could be in content:encoded, let's check here
    } else {
        // We might find the image data in either the content:encoded or the description part
        QString description;
        if (itemData.contains("content:encoded")) {
            description = itemData["content:encoded"].toString();
        }

        if (description.isNull() && itemData.contains("description")) {
            description = itemData["description"].toString();
        }

        if (!description.isNull()) {
            // Grab the first img tag in the description and use it as source
            int firstIndex = description.indexOf(QRegExp("<img [^>]*/>"));
            int sourceIndexStart = description.indexOf("src=\"", firstIndex) + 5;
            int sourceIndexEnd = description.indexOf('"', sourceIndexStart);

            QString imageUrl = description.mid(sourceIndexStart, sourceIndexEnd - sourceIndexStart);

            if (!imageUrl.isEmpty() && imageUrl.contains("http:")) {
                return imageUrl;
            }
        }
    }
    // We found nothing, return with sadness
    return "";
}

QString TLDRApp::plainText(const QString htmlString)
{
    // Remove all tags using a regular expression and returned the stripped string.
    QString plainString = htmlString;
    plainString.remove(QRegExp("<[^>]*>"));

    return plainString;
}

void TLDRApp::onSystemLanguageChanged()
{
    // Remove the old translator to make room for the new translation.
    QCoreApplication::instance()->removeTranslator(mTranslator);

    // Initiate, load and install the application translation files.
    QString localeString = QLocale().name();
    QString fileName = QString("tldr_%1").arg(localeString);
    if (mTranslator->load(fileName, "app/native/qm")) {
        QCoreApplication::instance()->installTranslator(mTranslator);
    }
}
