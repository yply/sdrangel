///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////


#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "limerfegui.h"
#endif
#include "limerfe.h"
#include "limerfeplugin.h"
#include "limerfewebapiadapter.h"

const PluginDescriptor LimeRFEPlugin::m_pluginDescriptor = {
    LimeRFE::m_featureId,
	QStringLiteral("LimeRFE USB Controller"),
    QStringLiteral("7.1.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

LimeRFEPlugin::LimeRFEPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& LimeRFEPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void LimeRFEPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register Simple PTT feature
	m_pluginAPI->registerFeature(LimeRFE::m_featureIdURI, LimeRFE::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* LimeRFEPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* LimeRFEPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return LimeRFEGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* LimeRFEPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new LimeRFE(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* LimeRFEPlugin::createFeatureWebAPIAdapter() const
{
	return new LimeRFEWebAPIAdapter();
}
