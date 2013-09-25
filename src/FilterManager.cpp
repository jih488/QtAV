/******************************************************************************
    QtAV:  Media play library based on Qt and FFmpeg
    Copyright (C) 2013 Wang Bin <wbsecg1@gmail.com>

*   This file is part of QtAV

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include "QtAV/FilterManager.h"
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtAV/AVPlayer.h>
#include <QtAV/Filter.h>
#include <QtAV/AVOutput.h>

namespace QtAV {

class FilterManagerPrivate : public DPtrPrivate<FilterManager>
{
public:
    FilterManagerPrivate()
    {}
    ~FilterManagerPrivate() {

    }

    QList<Filter*> pending_release_filters;
    QMap<Filter*, AVOutput*> filter_out_map;
    QMap<Filter*, AVPlayer*> filter_player_map;
};

FilterManager::FilterManager():
    QObject(0)
{
    connect(this, SIGNAL(uninstallInTargetDone(Filter*)), SLOT(onUninstallInTargetDone(Filter*)));
}

FilterManager::~FilterManager()
{
}

FilterManager& FilterManager::instance()
{
    static FilterManager sMgr;
    return sMgr;
}

bool FilterManager::registerFilter(Filter *filter, AVOutput *output)
{
    DPTR_D(FilterManager);
    d.pending_release_filters.removeAll(filter); //erase?
    if (d.filter_out_map.contains(filter) || d.filter_player_map.contains(filter)) {
        qWarning("Filter %p lready registered!", filter);
        return false;
    }
    d.filter_out_map.insert(filter, output);
    return true;
}

bool FilterManager::registerFilter(Filter *filter, AVPlayer *player)
{
    DPTR_D(FilterManager);
    d.pending_release_filters.removeAll(filter); //erase?
    if (d.filter_out_map.contains(filter) || d.filter_player_map.contains(filter)) {
        qWarning("Filter %p lready registered!", filter);
        return false;
    }
    d.filter_player_map.insert(filter, player);
    return true;
}

// called by AVOutput/AVThread.uninstall imediatly
bool FilterManager::unregisterFilter(Filter *filter)
{
    DPTR_D(FilterManager);
    return d.filter_out_map.remove(filter) || d.filter_player_map.remove(filter);
}

void FilterManager::onUninstallInTargetDone(Filter *filter)
{
    DPTR_D(FilterManager);
    //d.filter_out_map.remove(filter) || d.filter_player_map.remove(filter);
    //
    if (d.pending_release_filters.contains(filter)) {
        releaseFilterNow(filter);
    }
}

bool FilterManager::releaseFilter(Filter *filter)
{
    DPTR_D(FilterManager);
    d.pending_release_filters.push_back(filter);
    return uninstallFilter(filter);
    // will delete filter in slot onUninstallInTargetDone()(signal emitted when uninstall task done)
}

bool FilterManager::releaseFilterNow(Filter *filter)
{
    DPTR_D(FilterManager);
    int n = d.pending_release_filters.removeAll(filter);
    delete filter;
    return !!n;
}

bool FilterManager::uninstallFilter(Filter *filter)
{
    DPTR_D(FilterManager);
    QMap<Filter*,AVOutput*>::Iterator it = d.filter_out_map.find(filter);
    if (it != d.filter_out_map.end()) {
        AVOutput *out = *it;
        /*
         * remove now so that filter install again will success, even if unregister not completed
         * because the filter will never used in old target
         */

        d.filter_out_map.erase(it);
        out->uninstallFilter(filter); //post an uninstall task to active player's AVThread
        return true;
    }
    QMap<Filter*,AVPlayer*>::Iterator it2 = d.filter_player_map.find(filter);
    if (it2 != d.filter_player_map.end()) {
        AVPlayer *player = *it2;
        /*
         * remove now so that filter install again will success, even if unregister not completed
         * because the filter will never used in old target
         */

        d.filter_player_map.erase(it2);
        player->uninstallFilter(filter); //post an uninstall task to active player's AVThread
        return true;
    }
    return false;
}

} //namespace QtAV