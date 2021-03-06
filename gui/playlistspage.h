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

#ifndef PLAYLISTS_PAGE_H
#define PLAYLISTS_PAGE_H

#include "widgets/singlepagewidget.h"
#include "widgets/multipagewidget.h"
#include "models/playlistsproxymodel.h"

class Action;
class DynamicPage;

class StoredPlaylistsPage : public SinglePageWidget
{
    Q_OBJECT
public:
    StoredPlaylistsPage(QWidget *p);
    virtual ~StoredPlaylistsPage();

    void updateRows();
    void clear();
    //QStringList selectedFiles() const;
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priorty=0);
    void setView(int mode);
    #ifdef ENABLE_DEVICES_SUPPORT
    QList<Song> selectedSongs(bool allowPlaylists=false) const;
    void addSelectionToDevice(const QString &udi);
    #endif

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void loadPlaylist(const QString &name, bool replace);
    void removePlaylist(const QString &name);
    void savePlaylist(const QString &name);
    void renamePlaylist(const QString &oldname, const QString &newname);
    void removeFromPlaylist(const QString &name, const QList<quint32> &positions);

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);

private:
    void addItemsToPlayList(const QModelIndexList &indexes, const QString &name, int action, quint8 priorty=0);

public Q_SLOTS:
    void removeItems();

private Q_SLOTS:
    void savePlaylist();
    void renamePlaylist();
    void removeDuplicates();
    void itemDoubleClicked(const QModelIndex &index);
    void updated(const QModelIndex &index);
    void headerClicked(int level);
    void setStartClosed(bool sc);
    void updateToPlayQueue(const QModelIndex &idx, bool replace);

private:
    void doSearch();
    void controlActions();

private:
    Action *renamePlaylistAction;
    Action *removeDuplicatesAction;
    Action *intitiallyCollapseAction;
    PlaylistsProxyModel proxy;
};

class PlaylistsPage : public MultiPageWidget
{
    Q_OBJECT
public:
    PlaylistsPage(QWidget *p);
    virtual ~PlaylistsPage();

    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi);
    #endif

Q_SIGNALS:
    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);

private:
    StoredPlaylistsPage *stored;
    DynamicPage *dynamic;
};

#endif
