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

#include "dynamicruledialog.h"
#include "support/localize.h"
#include "support/monoicon.h"
#include "models/mpdlibrarymodel.h"

static const int constMinDate=1800;
static const int constMaxDate=2100;

DynamicRuleDialog::DynamicRuleDialog(QWidget *parent)
    : Dialog(parent)
    , addingRules(false)
{
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);
    setCaption(i18n("Dynamic Rule"));

    connect(artistText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(composerText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(commentText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(similarArtistsText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(albumArtistText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(albumText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(titleText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(genreText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(filenameText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(dateFromSpin, SIGNAL(valueChanged(int)), SLOT(enableOkButton()));
    connect(dateToSpin, SIGNAL(valueChanged(int)), SLOT(enableOkButton()));
    connect(exactCheck, SIGNAL(toggled(bool)), SLOT(enableOkButton()));

    QSet<QString> artists;
    QSet<QString> albumArtists;
    QSet<QString> composers;
    QSet<QString> albums;
    QSet<QString> genres;
    MpdLibraryModel::self()->getDetails(artists, albumArtists, composers, albums, genres);

    QStringList strings=artists.toList();
    strings.sort();
    artistText->clear();
    artistText->insertItems(0, strings);

    similarArtistsText->clear();
    similarArtistsText->insertItems(0, strings);

    strings=albumArtists.toList();
    strings.sort();
    albumArtistText->clear();
    albumArtistText->insertItems(0, strings);

    strings=composers.toList();
    strings.sort();
    composerText->clear();
    composerText->insertItems(0, strings);

    strings=albums.toList();
    strings.sort();
    albumText->clear();
    albumText->insertItems(0, strings);

    strings=genres.toList();
    strings.sort();
    genreText->clear();
    genreText->insertItems(0, strings);

    commentText->clear();

    dateFromSpin->setRange(constMinDate-1, constMaxDate);
    dateToSpin->setRange(constMinDate-1, constMaxDate);
    artistText->setFocus();
    errorLabel->setVisible(false);
    errorLabel->setStyleSheet(QLatin1String("QLabel{color:red;}"));
    adjustSize();
    int h=height();
    int w=width();
    int minW=Utils::scaleForDpi(500);
    setMinimumWidth(minW);
    setMinimumHeight(h);
    if (w<minW) {
        resize(minW, h);
    }
}

DynamicRuleDialog::~DynamicRuleDialog()
{
}

bool DynamicRuleDialog::edit(const Dynamic::Rule &rule, bool isAdd)
{
    addingRules=isAdd;
    typeCombo->setCurrentIndex(QLatin1String("true")==rule[Dynamic::constExcludeKey] ? 1 : 0);
    artistText->setText(rule[Dynamic::constArtistKey]);
    similarArtistsText->setText(rule[Dynamic::constSimilarArtistsKey]);
    albumArtistText->setText(rule[Dynamic::constAlbumArtistKey]);
    composerText->setText(rule[Dynamic::constComposerKey]);
    commentText->setText(rule[Dynamic::constCommentKey]);
    albumText->setText(rule[Dynamic::constAlbumKey]);
    titleText->setText(rule[Dynamic::constTitleKey]);
    genreText->setText(rule[Dynamic::constGenreKey]);
    filenameText->setText(rule[Dynamic::constFileKey]);

    QString date=rule[Dynamic::constDateKey];
    int dateFrom=0;
    int dateTo=0;
    if (!date.isEmpty()) {
        int idx=date.indexOf(Dynamic::constRangeSep);
        if (-1==idx) {
            dateFrom=date.toInt();
        } else {
            dateFrom=date.left(idx).toInt();
            dateTo=date.mid(idx+1).toInt();
        }
    }

    if (dateFrom<constMinDate || dateFrom>constMaxDate) {
        dateFrom=constMinDate-1;
    }
    if (dateTo<constMinDate || dateTo>constMaxDate) {
        dateTo=constMinDate-1;
    }
    dateFromSpin->setValue(dateFrom);
    dateToSpin->setValue(dateTo);
    exactCheck->setChecked(QLatin1String("false")!=rule[Dynamic::constExactKey]);
    errorLabel->setVisible(false);

    setButtons(isAdd ? User1|Ok|Close : Ok|Cancel);
    setButtonText(User1, i18n("Add"));
    setButtonGuiItem(User1, GuiItem(i18n("Add"),
                                    #ifdef ENABLE_KDE_SUPPORT
                                    "list-add"
                                    #else
                                    FontAwesome::plus
                                    #endif
                                    ));
    enableOkButton();
    return QDialog::Accepted==exec();
}

Dynamic::Rule DynamicRuleDialog::rule() const
{
    Dynamic::Rule r;
    if (!artist().isEmpty()) {
        r.insert(Dynamic::constArtistKey, artist());
    }
    if (!similarArtists().isEmpty()) {
        r.insert(Dynamic::constSimilarArtistsKey, similarArtists());
    }
    if (!albumArtist().isEmpty()) {
        r.insert(Dynamic::constAlbumArtistKey, albumArtist());
    }
    if (!composer().isEmpty()) {
        r.insert(Dynamic::constComposerKey, composer());
    }
    if (!comment().isEmpty()) {
        r.insert(Dynamic::constCommentKey, comment());
    }
    if (!album().isEmpty()) {
        r.insert(Dynamic::constAlbumKey, album());
    }
    if (!title().isEmpty()) {
        r.insert(Dynamic::constTitleKey, title());
    }
    if (!genre().isEmpty()) {
        r.insert(Dynamic::constGenreKey, genre());
    }
    if (!filename().isEmpty()) {
        r.insert(Dynamic::constFileKey, filename());
    }
    int dateFrom=dateFromSpin->value();
    int dateTo=dateToSpin->value();
    bool haveFrom=dateFrom>=constMinDate && dateFrom<=constMaxDate;
    bool haveTo=dateTo>=constMinDate && dateTo<=constMaxDate && dateTo!=dateFrom;

    if (haveFrom && haveTo) {
        r.insert(Dynamic::constDateKey, QString::number(dateFrom)+Dynamic::constRangeSep+QString::number(dateTo));
    } else if (haveFrom) {
        r.insert(Dynamic::constDateKey, QString::number(dateFrom));
    } else if (haveTo) {
        r.insert(Dynamic::constDateKey, QString::number(dateTo));
    }

    if (!exactCheck->isChecked()) {
        r.insert(Dynamic::constExactKey, QLatin1String("false"));
    }
    if (1==typeCombo->currentIndex()) {
        r.insert(Dynamic::constExcludeKey, QLatin1String("true"));
    }
    return r;
}

void DynamicRuleDialog::enableOkButton()
{
    static const int constMaxDateRange=20;

    int dateFrom=dateFromSpin->value();
    int dateTo=dateToSpin->value();
    bool haveFrom=dateFrom>=constMinDate && dateFrom<=constMaxDate;
    bool haveTo=dateTo>=constMinDate && dateTo<=constMaxDate && dateTo!=dateFrom;
    bool enable=(!haveFrom || !haveTo || (dateTo>=dateFrom && (dateTo-dateFrom)<=constMaxDateRange)) &&
                (haveFrom || haveTo || !artist().isEmpty() || !similarArtists().isEmpty() || !albumArtist().isEmpty() ||
                 !composer().isEmpty() || !comment().isEmpty() || !album().isEmpty() || !title().isEmpty() || !genre().isEmpty() || !filename().isEmpty());

    if (enable && exactCheck->isChecked() && !filename().isEmpty()) {
        enable=false;
    }

    errorLabel->setVisible(false);
    if (!enable) {
        if (haveFrom && haveTo) {
            if (dateTo<dateFrom) {
                errorLabel->setText(i18n("<i><b>ERROR</b>: 'From Year' should be less than 'To Year'</i>"));
                errorLabel->setVisible(true);
            } else if (dateTo-dateFrom>constMaxDateRange) {
                errorLabel->setText(i18n("<i><b>ERROR:</b> Date range is too large (can only be a maximum of %1 years)</i>", constMaxDateRange));
                errorLabel->setVisible(true);
            }
        }
        if (!filename().isEmpty() && exactCheck->isChecked() && !errorLabel->isVisible()) {
            errorLabel->setText(i18n("<i><b>ERROR:</b> You can only match on filename / path if 'Exact match' is <b>not</b> checked</i>"));
            errorLabel->setVisible(true);
        }
    }
    enableButton(Ok, enable);
    if (addingRules) {
        enableButton(User1, enable);
    }
}

void DynamicRuleDialog::slotButtonClicked(int button)
{
    if (addingRules && (User1==button || Ok==button)) {
        emit addRule(rule());
    }

    Dialog::slotButtonClicked(button);
}
