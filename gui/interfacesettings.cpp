/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "interfacesettings.h"
#include "settings.h"
#include "models/sqllibrarymodel.h"
#include "support/localize.h"
#include "support/utils.h"
#include "support/fancytabwidget.h"
#include "support/pathrequester.h"
#include "widgets/basicitemdelegate.h"
#include "widgets/playqueueview.h"
#include "widgets/itemview.h"
#include "db/librarydb.h"
#include <QComboBox>
#ifndef ENABLE_KDE_SUPPORT
#include <QDir>
#include <QMap>
#include <QRegExp>
#include <QSet>
#endif
#ifdef QT_QTDBUS_FOUND
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif
#ifndef ENABLE_KDE_SUPPORT
#include <QSystemTrayIcon>
#endif
#include <QSysInfo>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

static QString viewTypeString(ItemView::Mode mode)
{
    switch (mode) {
    default:
    case ItemView::Mode_GroupedTree:  return i18n("Grouped Albums");
    case ItemView::Mode_Table:        return i18n("Table");
    }
}

static void addViewTypes(QComboBox *box, QList<ItemView::Mode> modes)
{
    foreach (ItemView::Mode m, modes) {
        box->addItem(viewTypeString(m), m);
    }
}

static QString cueSupportString(MPDParseUtils::CueSupport cs)
{
    switch (cs) {
    default:
    case MPDParseUtils::Cue_Parse:            return i18n("Parse in Library view, and show in Folders view");
    case MPDParseUtils::Cue_ListButDontParse: return i18n("Only show in Folders view");
    case MPDParseUtils::Cue_Ignore:           return i18n("Do not list");
    }
}

static void addCueSupportTypes(QComboBox *box)
{
    for (int i=0; i<MPDParseUtils::Cue_Count; ++i) {
        box->addItem(cueSupportString((MPDParseUtils::CueSupport)i), i);
    }
}

static void selectEntry(QComboBox *box, int v)
{
    for (int i=1; i<box->count(); ++i) {
        if (box->itemData(i).toInt()==v) {
            box->setCurrentIndex(i);
            return;
        }
    }
}

static inline int getValue(QComboBox *box)
{
    return box->itemData(box->currentIndex()).toInt();
}

static inline QString getStrValue(QComboBox *box)
{
    return box->itemData(box->currentIndex()).toString();
}

static const char * constValueProperty="value";
static const char * constSep=",";

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
    #ifndef ENABLE_KDE_SUPPORT
    , loadedLangs(false)
    #endif
{
    #ifdef Q_OS_MAC
    // OSX always displays an entry in the taskbar - and the tray seems to confuse things.
    bool enableTrayItem=false;
    bool enableNotifications=QSysInfo::MacintoshVersion >= QSysInfo::MV_10_8;
    #else
    #ifdef QT_QTDBUS_FOUND
    // We have dbus, check that org.freedesktop.Notifications exists
    bool enableNotifications=QDBusConnection::sessionBus().interface()->isServiceRegistered("org.freedesktop.Notifications");
    #else // QT_QTDBUS_FOUND
    bool enableNotifications=true;
    #endif // QT_QTDBUS_FOUND
    bool enableTrayItem=true;
    #ifndef ENABLE_KDE_SUPPORT
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        enableTrayItem=false;
        #ifndef QT_QTDBUS_FOUND
        enableNotifications=false;
        #endif
    }
    #endif // !ENABLE_KDE_SUPPORT
    #endif // Q_MAC_OS

    setupUi(this);
    addCueSupportTypes(cueSupport);
    addViewTypes(playQueueView, QList<ItemView::Mode>() << ItemView::Mode_GroupedTree << ItemView::Mode_Table);

    addView(i18n("Play Queue"), QLatin1String("PlayQueuePage"));
    addView(i18n("Library"), QLatin1String("LibraryPage"));
    addView(i18n("Folders"), QLatin1String("FolderPage"));
    addView(i18n("Playlists"), QLatin1String("PlaylistsPage"));
    addView(i18n("Dynamic Playlists"), QLatin1String("DynamicPage"));
    addView(i18n("Internet - Streams, Jamendo, Maganatune, SoundCloud, and Podcasts"), QLatin1String("OnlineServicesPage"));
    #ifdef ENABLE_DEVICES_SUPPORT
    addView(i18n("Devices - UMS, MTP (e.g. Android), and AudioCDs"), QLatin1String("DevicesPage"));
    #else
    REMOVE(showDeleteAction)
    #endif
    addView(i18n("Search (via MPD)"), QLatin1String("SearchPage"));
    addView(i18n("Info - Current song information (artist, album, and lyrics)"), QLatin1String("ContextPage"));
    connect(playQueueView, SIGNAL(currentIndexChanged(int)), SLOT(playQueueViewChanged()));
    connect(forceSingleClick, SIGNAL(toggled(bool)), SLOT(forceSingleClickChanged()));
    #ifdef ENABLE_TOUCH_SUPPORT
    connect(touchFriendly, SIGNAL(toggled(bool)), SLOT(touchFriendlyChanged()));
    #else
    REMOVE(touchFriendly)
    REMOVE(touchFriendlyNoteLabel)
    #endif
    connect(views, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(viewItemChanged(QListWidgetItem*)));

    #ifdef ENABLE_KDE_SUPPORT
    REMOVE(lang)
    REMOVE(langLabel)
    REMOVE(langNoteLabel)
    #endif

    sbStyle->addItem(i18n("Large"), FancyTabWidget::Large);
    sbStyle->addItem(i18n("Small"), FancyTabWidget::Small);
    sbStyle->addItem(i18n("Tab-bar"), FancyTabWidget::Tab);
    sbPosition->addItem(Qt::LeftToRight==layoutDirection() ? i18n("Left") : i18n("Right"), FancyTabWidget::Side);
    sbPosition->addItem(i18n("Top"), FancyTabWidget::Top);
    sbPosition->addItem(i18n("Bottom"), FancyTabWidget::Bot);
    connect(sbAutoHide, SIGNAL(toggled(bool)), SLOT(sbAutoHideChanged()));
    views->setItemDelegate(new BasicItemDelegate(views));
    playQueueBackground_none->setProperty(constValueProperty, PlayQueueView::BI_None);
    playQueueBackground_cover->setProperty(constValueProperty, PlayQueueView::BI_Cover);
    playQueueBackground_custom->setProperty(constValueProperty, PlayQueueView::BI_Custom);
    playQueueBackgroundFile->setDirMode(false);
    #ifdef ENABLE_KDE_SUPPORT
    playQueueBackgroundFile->setFilter("image/jpeg image/png");
    #else
    playQueueBackgroundFile->setFilter(i18n("Images (*.png *.jpg)"));
    #endif
    int labelWidth=qMax(fontMetrics().width(QLatin1String("100%")), fontMetrics().width(i18nc("pixels", "10px")));
    playQueueBackgroundOpacityLabel->setFixedWidth(labelWidth);
    playQueueBackgroundBlurLabel->setFixedWidth(labelWidth);
    connect(playQueueBackgroundOpacity, SIGNAL(valueChanged(int)), SLOT(setPlayQueueBackgroundOpacityLabel()));
    connect(playQueueBackgroundBlur, SIGNAL(valueChanged(int)), SLOT(setPlayQueueBackgroundBlurLabel()));
    connect(playQueueBackground_none, SIGNAL(toggled(bool)), SLOT(enablePlayQueueBackgroundOptions()));
    connect(playQueueBackground_cover, SIGNAL(toggled(bool)), SLOT(enablePlayQueueBackgroundOptions()));
    connect(playQueueBackground_custom, SIGNAL(toggled(bool)), SLOT(enablePlayQueueBackgroundOptions()));

    if (!enableNotifications) {
        REMOVE(systemTrayPopup)
    }
    if (enableTrayItem) {
        connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnClose, SLOT(setEnabled(bool)));
        connect(systemTrayCheckBox, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
        connect(minimiseOnClose, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    } else {
        REMOVE(systemTrayCheckBox)
        REMOVE(minimiseOnClose)
        REMOVE(startupState)
    }

    if (!enableNotifications && !enableTrayItem) {
        tabWidget->removeTab(3);
    } else if (!enableTrayItem && enableNotifications) {
        tabWidget->setTabText(3, i18n("Notifications"));
    }
    #if !defined QT_QTDBUS_FOUND
    REMOVE(enableMpris)
    #endif
    #if defined Q_OS_WIN || defined Q_OS_MAC || !defined QT_QTDBUS_FOUND
    if (systemTrayPopup && systemTrayCheckBox) {
        connect(systemTrayCheckBox, SIGNAL(toggled(bool)), SLOT(systemTrayCheckBoxToggled()));
        connect(systemTrayPopup, SIGNAL(toggled(bool)), SLOT(systemTrayPopupToggled()));
    }
    #endif
    #if QT_VERSION >= 0x050400 || (defined Q_OS_MAC && QT_VERSION >= 0x050100)
    connect(retinaSupport, SIGNAL(toggled(bool)), SLOT(retinaSupportChanged()));
    #else
    REMOVE(retinaSupport)
    REMOVE(retinaSupportNoteLabel)
    #endif
}

void InterfaceSettings::load()
{
    ignorePrefixes->setText(QStringList(Settings::self()->ignorePrefixes().toList()).join(QString(constSep)));
    composerGenres->setText(QStringList(Settings::self()->composerGenres().toList()).join(QString(constSep)));
    singleTracksFolders->setText(QStringList(Settings::self()->singleTracksFolders().toList()).join(QString(constSep)));
    selectEntry(cueSupport, Settings::self()->cueSupport());
    #ifdef ENABLE_DEVICES_SUPPORT
    showDeleteAction->setChecked(Settings::self()->showDeleteAction());
    #endif

    selectEntry(playQueueView, Settings::self()->playQueueView());
    playQueueAutoExpand->setChecked(Settings::self()->playQueueAutoExpand());
    playQueueStartClosed->setChecked(Settings::self()->playQueueStartClosed());
    playQueueScroll->setChecked(Settings::self()->playQueueScroll());

    int pqBgnd=Settings::self()->playQueueBackground();
    playQueueBackground_none->setChecked(pqBgnd==playQueueBackground_none->property(constValueProperty).toInt());
    playQueueBackground_cover->setChecked(pqBgnd==playQueueBackground_cover->property(constValueProperty).toInt());
    playQueueBackground_custom->setChecked(pqBgnd==playQueueBackground_custom->property(constValueProperty).toInt());
    playQueueBackgroundOpacity->setValue(Settings::self()->playQueueBackgroundOpacity());
    playQueueBackgroundBlur->setValue(Settings::self()->playQueueBackgroundBlur());
    playQueueBackgroundFile->setText(Utils::convertPathForDisplay(Settings::self()->playQueueBackgroundFile(), false));

    playQueueConfirmClear->setChecked(Settings::self()->playQueueConfirmClear());
    playQueueSearch->setChecked(Settings::self()->playQueueSearch());
    playQueueViewChanged();
    forceSingleClick->setChecked(Settings::self()->forceSingleClick());
    infoTooltips->setChecked(Settings::self()->infoTooltips());
    if (retinaSupport) {
        retinaSupport->setChecked(Settings::self()->retinaSupport());
    }
    if (touchFriendly) {
        touchFriendly->setChecked(Settings::self()->touchFriendly());
    }
    showStopButton->setChecked(Settings::self()->showStopButton());
    showCoverWidget->setChecked(Settings::self()->showCoverWidget());
    showRatingWidget->setChecked(Settings::self()->showRatingWidget());
    if (systemTrayCheckBox) {
        systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
        if (minimiseOnClose) {
            minimiseOnClose->setChecked(Settings::self()->minimiseOnClose());
            minimiseOnClose->setEnabled(systemTrayCheckBox->isChecked());
        }
        if (startupState) {
            switch (Settings::self()->startupState()) {
            case Settings::SS_ShowMainWindow: startupStateShow->setChecked(true); break;
            case Settings::SS_HideMainWindow: startupStateHide->setChecked(true); break;
            case Settings::SS_Previous: startupStateRestore->setChecked(true); break;
            }

            enableStartupState();
        }
    }
    if (systemTrayPopup) {
        systemTrayPopup->setChecked(Settings::self()->showPopups());
    }
    fetchCovers->setChecked(Settings::self()->fetchCovers());

    QStringList hiddenPages=Settings::self()->hiddenPages();
    for (int i=0; i<views->count(); ++i) {
        QListWidgetItem *v=views->item(i);
        v->setCheckState(hiddenPages.contains(v->data(Qt::UserRole).toString()) ? Qt::Unchecked : Qt::Checked);
    }
    int sidebar=Settings::self()->sidebar();
    selectEntry(sbStyle, sidebar&FancyTabWidget::Style_Mask);
    selectEntry(sbPosition, sidebar&FancyTabWidget::Position_Mask);
    sbIconsOnly->setChecked(sidebar&FancyTabWidget::IconOnly);
    sbAutoHide->setChecked(Settings::self()->splitterAutoHide());
    sbAutoHideChanged();
    viewItemChanged(views->item(0));
    setPlayQueueBackgroundOpacityLabel();
    setPlayQueueBackgroundBlurLabel();
    enablePlayQueueBackgroundOptions();
    if (enableMpris) {
        enableMpris->setChecked(Settings::self()->mpris());
    }
}

static QSet<QString> toSet(const QString &str)
{
    QStringList parts=str.split(constSep, QString::SkipEmptyParts);
    QSet<QString> set;
    foreach (QString s, parts) {
        set.insert(s.trimmed());
    }
    return set;
}

void InterfaceSettings::save()
{
    Settings::self()->saveIgnorePrefixes(toSet(ignorePrefixes->text()));
    Settings::self()->saveComposerGenres(toSet(composerGenres->text()));
    Settings::self()->saveSingleTracksFolders(toSet(singleTracksFolders->text()));
    Settings::self()->saveCueSupport((MPDParseUtils::CueSupport)(cueSupport->itemData(cueSupport->currentIndex()).toInt()));
    #ifdef ENABLE_DEVICES_SUPPORT
    Settings::self()->saveShowDeleteAction(showDeleteAction->isChecked());
    #endif
    Settings::self()->savePlayQueueView(getValue(playQueueView));
    Settings::self()->savePlayQueueAutoExpand(playQueueAutoExpand->isChecked());
    Settings::self()->savePlayQueueStartClosed(playQueueStartClosed->isChecked());
    Settings::self()->savePlayQueueScroll(playQueueScroll->isChecked());

    if (playQueueBackground_none->isChecked()) {
        Settings::self()->savePlayQueueBackground(playQueueBackground_none->property(constValueProperty).toInt());
    } else if (playQueueBackground_cover->isChecked()) {
        Settings::self()->savePlayQueueBackground(playQueueBackground_cover->property(constValueProperty).toInt());
    } else if (playQueueBackground_custom->isChecked()) {
        Settings::self()->savePlayQueueBackground(playQueueBackground_custom->property(constValueProperty).toInt());
    }
    Settings::self()->savePlayQueueBackgroundOpacity(playQueueBackgroundOpacity->value());
    Settings::self()->savePlayQueueBackgroundBlur(playQueueBackgroundBlur->value());
    Settings::self()->savePlayQueueBackgroundFile(Utils::convertPathFromDisplay(playQueueBackgroundFile->text(), false));

    Settings::self()->savePlayQueueConfirmClear(playQueueConfirmClear->isChecked());
    Settings::self()->savePlayQueueSearch(playQueueSearch->isChecked());
    Settings::self()->saveForceSingleClick(forceSingleClick->isChecked());
    Settings::self()->saveInfoTooltips(infoTooltips->isChecked());
    if (retinaSupport) {
        Settings::self()->saveRetinaSupport(retinaSupport->isChecked());
    }
    if (touchFriendly) {
        Settings::self()->saveTouchFriendly(touchFriendly->isChecked());
    }
    Settings::self()->saveShowStopButton(showStopButton->isChecked());
    Settings::self()->saveShowCoverWidget(showCoverWidget->isChecked());
    Settings::self()->saveShowRatingWidget(showRatingWidget->isChecked());
    Settings::self()->saveUseSystemTray(systemTrayCheckBox && systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup && systemTrayPopup->isChecked());
    Settings::self()->saveMinimiseOnClose(minimiseOnClose && minimiseOnClose->isChecked());
    if (!startupState || startupStateShow->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_ShowMainWindow);
    } else if (startupStateHide->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_HideMainWindow);
    } else if (startupStateRestore->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_Previous);
    }
    Settings::self()->saveFetchCovers(fetchCovers->isChecked());
    #ifndef ENABLE_KDE_SUPPORT
    if (loadedLangs && lang) {
        Settings::self()->saveLang(lang->itemData(lang->currentIndex()).toString());
    }
    #endif

    QStringList hiddenPages;
    for (int i=0; i<views->count(); ++i) {
        QListWidgetItem *v=views->item(i);
        if (Qt::Unchecked==v->checkState()) {
            hiddenPages.append(v->data(Qt::UserRole).toString());
        }
    }
    Settings::self()->saveHiddenPages(hiddenPages);
    int sidebar=getValue(sbStyle)|getValue(sbPosition);
    if (sbIconsOnly->isChecked()) {
        sidebar|=FancyTabWidget::IconOnly;
    }
    Settings::self()->saveSidebar(sidebar);
    Settings::self()->saveSplitterAutoHide(sbAutoHide->isChecked());
    if (enableMpris) {
        Settings::self()->saveMpris(enableMpris->isChecked());
    }
}

#ifndef ENABLE_KDE_SUPPORT
static bool localeAwareCompare(const QString &a, const QString &b)
{
    return a.localeAwareCompare(b) < 0;
}

static QSet<QString> translationCodes(const QString &dir)
{
    QSet<QString> codes;
    QDir d(dir);
    QStringList installed(d.entryList(QStringList() << "*.qm"));
    QRegExp langRegExp("^cantata_(.*).qm$");
    foreach (const QString &filename, installed) {
        if (langRegExp.exactMatch(filename)) {
            codes.insert(langRegExp.cap(1));
        }
    }
    return codes;
}

void InterfaceSettings::showEvent(QShowEvent *e)
{
    if (!loadedLangs) {
        loadedLangs=true;

        QMap<QString, QString> langMap;
        QSet<QString> transCodes;

        transCodes+=translationCodes(qApp->applicationDirPath()+QLatin1String("/translations"));
        transCodes+=translationCodes(QDir::currentPath()+QLatin1String("/translations"));
        #ifndef Q_OS_WIN
        transCodes+=translationCodes(CANTATA_SYS_TRANS_DIR);
        #endif

        foreach (const QString &code, transCodes) {
            QString langName = QLocale::languageToString(QLocale(code).language());
            #if QT_VERSION >= 0x040800
            QString nativeName = QLocale(code).nativeLanguageName();
            if (!nativeName.isEmpty()) {
                langName = nativeName;
            }
            #endif
            langMap[QString("%1 (%2)").arg(langName, code)] = code;
        }

        langMap["English (en)"] = "en";

        QString current = Settings::self()->lang();
        QStringList names = langMap.keys();
        qStableSort(names.begin(), names.end(), localeAwareCompare);
        lang->addItem(i18n("System default"), QString());
        lang->setCurrentIndex(0);
        foreach (const QString &name, names) {
            lang->addItem(name, langMap[name]);
            if (langMap[name]==current) {
                lang->setCurrentIndex(lang->count()-1);
            }
        }

        if (lang->count()<3) {
            REMOVE(lang)
            REMOVE(langLabel)
            REMOVE(langNoteLabel)
        } else {
            connect(lang, SIGNAL(currentIndexChanged(int)), SLOT(langChanged()));
        }
    }
    QWidget::showEvent(e);
}
#endif

void InterfaceSettings::showPage(const QString &page)
{
    if (QLatin1String("sidebar")==page) {
        tabWidget->setCurrentIndex(0);
    }
}

QSize InterfaceSettings::sizeHint() const
{
    QSize sz=QWidget::sizeHint();
    #ifdef Q_OS_MAC
    sz.setWidth(sz.width()+32);
    sz.setHeight(qMin(sz.height(), 500));
    #endif
    return sz;
}

void InterfaceSettings::addView(const QString &v, const QString &prop)
{
    QListWidgetItem *item=new QListWidgetItem(v, views);
    item->setCheckState(Qt::Unchecked);
    item->setData(Qt::UserRole, prop);
}

void InterfaceSettings::playQueueViewChanged()
{
    bool grouped=ItemView::Mode_GroupedTree==getValue(playQueueView);
    playQueueAutoExpand->setEnabled(grouped);
    playQueueStartClosed->setEnabled(grouped);
}

void InterfaceSettings::forceSingleClickChanged()
{
    singleClickLabel->setOn(forceSingleClick->isChecked()!=Settings::self()->forceSingleClick());
}

void InterfaceSettings::touchFriendlyChanged()
{
    touchFriendlyNoteLabel->setOn(touchFriendly->isChecked()!=Settings::self()->touchFriendly());
}

void InterfaceSettings::retinaSupportChanged()
{
    retinaSupportNoteLabel->setOn(retinaSupport->isChecked()!=Settings::self()->retinaSupport());
}

void InterfaceSettings::enableStartupState()
{
    if (systemTrayCheckBox && minimiseOnClose && startupState) {
        startupState->setEnabled(systemTrayCheckBox->isChecked() && minimiseOnClose->isChecked());
    }
}

void InterfaceSettings::langChanged()
{
    #ifndef ENABLE_KDE_SUPPORT
    langNoteLabel->setOn(lang->itemData(lang->currentIndex()).toString()!=Settings::self()->lang());
    #endif
}

void InterfaceSettings::viewItemChanged(QListWidgetItem *changedItem)
{
    // If this is the playqueue that has been toggled, then control auto-hide
    // i.e. can't auto-hide if playqueue is in sidebar
    if (Qt::Checked==changedItem->checkState() && changedItem==views->item(0)) {
        sbAutoHide->setChecked(false);
    }

    // Ensure we have at least 1 view checked...
    for (int i=0; i<views->count(); ++i) {
        QListWidgetItem *v=views->item(i);
        if (Qt::Checked==v->checkState()) {
            return;
        }
    }

    views->item(1)->setCheckState(Qt::Checked);
}

void InterfaceSettings::sbAutoHideChanged()
{
    if (sbAutoHide->isChecked()) {
        views->item(0)->setCheckState(Qt::Unchecked);
    }
}

void InterfaceSettings::setPlayQueueBackgroundOpacityLabel()
{
    playQueueBackgroundOpacityLabel->setText(i18nc("value%", "%1%", playQueueBackgroundOpacity->value()));
}

void InterfaceSettings::setPlayQueueBackgroundBlurLabel()
{
    playQueueBackgroundBlurLabel->setText(i18nc("pixels", "%1 px", playQueueBackgroundBlur->value()));
}

void InterfaceSettings::enablePlayQueueBackgroundOptions()
{
    playQueueBackgroundOpacity->setEnabled(!playQueueBackground_none->isChecked());
    playQueueBackgroundOpacityLabel->setEnabled(playQueueBackgroundOpacity->isEnabled());
    playQueueBackgroundBlur->setEnabled(playQueueBackgroundOpacity->isEnabled());
    playQueueBackgroundBlurLabel->setEnabled(playQueueBackgroundOpacity->isEnabled());
}

void InterfaceSettings::systemTrayCheckBoxToggled()
{
    if (systemTrayCheckBox && systemTrayPopup && !systemTrayCheckBox->isChecked()) {
        systemTrayPopup->setChecked(false);
    }
}

void InterfaceSettings::systemTrayPopupToggled()
{
    if (systemTrayCheckBox && systemTrayPopup && systemTrayPopup->isChecked()) {
        systemTrayCheckBox->setChecked(true);
    }
}
