///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <cmath>

#include "device/deviceuiset.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QQuickItem>
#include <QGeoLocation>
#include <QQmlContext>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QDebug>
#include <QProcess>
#include <QClipboard>
#include <QFileDialog>
#include <QQmlProperty>
#include <QJsonDocument>
#include <QJsonObject>

#include <QtGui/private/qzipreader_p.h>

#include "ui_adsbdemodgui.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/units.h"
#include "util/morse.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/clickablelabel.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"

#include "adsbdemodreport.h"
#include "adsbdemod.h"
#include "adsbdemodgui.h"
#include "adsbdemodfeeddialog.h"
#include "adsbdemoddisplaydialog.h"
#include "adsbdemodnotificationdialog.h"
#include "adsb.h"
#include "adsbosmtemplateserver.h"

const char *Aircraft::m_speedTypeNames[] = {
    "GS", "TAS", "IAS"
};

ADSBDemodGUI* ADSBDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ADSBDemodGUI* gui = new ADSBDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void ADSBDemodGUI::destroy()
{
    delete this;
}

void ADSBDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray ADSBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ADSBDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        updateDeviceSetList();
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

// Longitude zone (returns value in range [1,59]
static int cprNL(double lat)
{
    if (lat == 0.0)
    {
        return 59;
    }
    else if ((lat == 87.0) || (lat == -87.0))
    {
        return 2;
    }
    else if ((lat > 87.0) || (lat < -87.0))
    {
        return 1;
    }
    else
    {
        double nz = 15.0;
        double n = 1 - std::cos(M_PI / (2.0 * nz));
        double d = std::cos(std::fabs(lat) * M_PI/180.0);
        return std::floor((M_PI * 2.0) / std::acos(1.0 - (n/(d*d))));
    }
}

static int cprN(double lat, int odd)
{
    int nl = cprNL(lat) - odd;
    if (nl > 1) {
        return nl;
    } else {
        return 1;
    }
}

// Can't use std::fmod, as that works differently for negative numbers (See C.2.6.2)
static Real modulus(double x, double y)
{
    return x - y * std::floor(x/y);
}

QString Aircraft::getImage() const
{
    if (m_emitterCategory.length() > 0)
    {
        if (!m_emitterCategory.compare("Heavy")) {
            return QString("aircraft_4engine.png"); // Can also be 777, 787
        } else if (!m_emitterCategory.compare("Large")) {
            return QString("aircraft_2engine.png");
        } else if (!m_emitterCategory.compare("Small")) {
            return QString("aircraft_2enginesmall.png");
        } else if (!m_emitterCategory.compare("Rotorcraft")) {
            return QString("aircraft_helicopter.png");
        } else if (!m_emitterCategory.compare("High performance")) {
            return QString("aircraft_fighter.png");
        } else if (!m_emitterCategory.compare("Light")
                || !m_emitterCategory.compare("Ultralight")
                || !m_emitterCategory.compare("Glider/sailplane")) {
            return QString("aircraft_light.png");
        } else if (!m_emitterCategory.compare("Space vehicle")) {
            return QString("aircraft_space.png");
        } else if (!m_emitterCategory.compare("UAV")) {
            return QString("aircraft_drone.png");
        } else if (!m_emitterCategory.compare("Emergency vehicle")
                || !m_emitterCategory.compare("Service vehicle")) {
            return QString("truck.png");
        } else {
            return QString("aircraft_2engine.png");
        }
    }
    else
    {
        return QString("aircraft_2engine.png");
    }
}

QString Aircraft::getText(bool all) const
{
    QStringList list;
    if (m_showAll || all)
    {
        if (!m_flagIconURL.isEmpty() && !m_airlineIconURL.isEmpty())
        {
            list.append(QString("<table width=100%><tr><td><img src=%1><td><img src=%2 align=right></table>").arg(m_airlineIconURL).arg(m_flagIconURL));
        }
        else
        {
            if (!m_flagIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_flagIconURL));
            } else if (!m_airlineIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_airlineIconURL));
            }
        }
        list.append(QString("ICAO: %1").arg(m_icaoHex));
        if (!m_callsign.isEmpty()) {
            list.append(QString("Callsign: %1").arg(m_callsign));
        }
        if (m_aircraftInfo != nullptr)
        {
            if (!m_aircraftInfo->m_model.isEmpty()) {
                list.append(QString("Aircraft: %1").arg(m_aircraftInfo->m_model));
            }
        }
        if (!m_emitterCategory.isEmpty()) {
            list.append(QString("Category: %1").arg(m_emitterCategory));
        }
        if (m_altitudeValid)
        {
            if (m_onSurface)
            {
                list.append(QString("Altitude: Surface"));
            }
            else
            {
                QString reference = m_altitudeGNSS ? "GNSS" : "Baro";
                if (m_gui->useSIUints()) {
                    list.append(QString("Altitude: %1 (m %2)").arg(Units::feetToIntegerMetres(m_altitude)).arg(reference));
                } else {
                    list.append(QString("Altitude: %1 (ft %2)").arg(m_altitude).arg(reference));
                }
            }
        }
        if (m_speedValid)
        {
            if (m_gui->useSIUints()) {
                list.append(QString("%1: %2 (kph)").arg(m_speedTypeNames[m_speedType]).arg(Units::knotsToIntegerKPH(m_speed)));
            } else {
                list.append(QString("%1: %2 (kn)").arg(m_speedTypeNames[m_speedType]).arg(m_speed));
            }
        }
        if (m_verticalRateValid)
        {
            QString desc;
            Real rate;
            QString units;

            if (m_gui->useSIUints())
            {
                rate = Units::feetPerMinToIntegerMetresPerSecond(m_verticalRate);
                units = QString("m/s");
            }
            else
            {
                rate = m_verticalRate;
                units = QString("ft/min");
            }
            if (m_verticalRate == 0) {
                desc = "Level flight";
            } else if (rate > 0) {
                desc = QString("Climbing: %1 (%2)").arg(rate).arg(units);
            } else {
                desc = QString("Descending: %1 (%2)").arg(rate).arg(units);
            }
            list.append(QString(desc));
        }
        if ((m_status.length() > 0) && m_status.compare("No emergency")) {
            list.append(m_status);
        }

        QString flightStatus = m_flightStatusItem->text();
        if (!flightStatus.isEmpty()) {
            list.append(QString("Flight status: %1").arg(flightStatus));
        }
        QString dep = m_depItem->text();
        if (!dep.isEmpty()) {
            list.append(QString("Departed: %1").arg(dep));
        }
        QString std = m_stdItem->text();
        if (!std.isEmpty()) {
            list.append(QString("STD: %1").arg(std));
        }
        QString atd = m_atdItem->text();
        if (!atd.isEmpty())
        {
            list.append(QString("ATD: %1").arg(atd));
        }
        else
        {
            QString etd = m_etdItem->text();
            if (!etd.isEmpty()) {
                list.append(QString("ETD: %1").arg(etd));
            }
        }
        QString arr = m_arrItem->text();
        if (!arr.isEmpty()) {
            list.append(QString("Arrival: %1").arg(arr));
        }
        QString sta = m_staItem->text();
        if (!sta.isEmpty()) {
            list.append(QString("STA: %1").arg(sta));
        }
        QString ata = m_ataItem->text();
        if (!ata.isEmpty())
        {
            list.append(QString("ATA: %1").arg(ata));
        }
        else
        {
            QString eta = m_etaItem->text();
            if (!eta.isEmpty()) {
                list.append(QString("ETA: %1").arg(eta));
            }
        }
    }
    else if (!m_callsign.isEmpty())
    {
        list.append(m_callsign);
    }
    else
    {
        list.append(m_icaoHex);
    }
    return list.join("<br>");
}

QVariant AircraftModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_aircrafts.count()))
        return QVariant();
    if (role == AircraftModel::positionRole)
    {
        // Coordinates to display the aircraft icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_aircrafts[row]->m_latitude);
        coords.setLongitude(m_aircrafts[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_aircrafts[row]->m_altitude));
        return QVariant::fromValue(coords);
    }
    else if (role == AircraftModel::headingRole)
    {
        // What rotation to draw the aircraft icon at
        return QVariant::fromValue(m_aircrafts[row]->m_heading);
    }
    else if (role == AircraftModel::adsbDataRole)
    {
        // Create the text to go in the bubble next to the aircraft
        return QVariant::fromValue(m_aircrafts[row]->getText());
    }
    else if (role == AircraftModel::aircraftImageRole)
    {
        // Select an image to use for the aircraft
        return QVariant::fromValue(m_aircrafts[row]->getImage());
    }
    else if (role == AircraftModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the aircraft
        if (m_aircrafts[row]->m_isTarget)
            return  QVariant::fromValue(QColor("lightgreen"));
        else if (m_aircrafts[row]->m_isHighlighted)
            return  QVariant::fromValue(QColor("orange"));
        else if ((m_aircrafts[row]->m_status.length() > 0) && m_aircrafts[row]->m_status.compare("No emergency"))
            return QVariant::fromValue(QColor("lightred"));
        else
            return QVariant::fromValue(QColor("lightblue"));
    }
    else if (role == AircraftModel::aircraftPathRole)
    {
       if ((m_flightPaths && m_aircrafts[row]->m_isHighlighted) || m_allFlightPaths)
           return m_aircrafts[row]->m_coordinates;
       else
           return QVariantList();
    }
    else if (role == AircraftModel::showAllRole)
        return QVariant::fromValue(m_aircrafts[row]->m_showAll);
    else if (role == AircraftModel::highlightedRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isHighlighted);
    else if (role == AircraftModel::targetRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isTarget);
    return QVariant();
}

bool AircraftModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_aircrafts.count()))
        return false;
    if (role == AircraftModel::showAllRole)
    {
        bool showAll = value.toBool();
        if (showAll != m_aircrafts[row]->m_showAll)
        {
            m_aircrafts[row]->m_showAll = showAll;
            emit dataChanged(index, index);
        }
        return true;
    }
    else if (role == AircraftModel::highlightedRole)
    {
        bool highlight = value.toBool();
        if (highlight != m_aircrafts[row]->m_isHighlighted)
        {
            m_aircrafts[row]->m_gui->highlightAircraft(m_aircrafts[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    else if (role == AircraftModel::targetRole)
    {
        bool target = value.toBool();
        if (target != m_aircrafts[row]->m_isTarget)
        {
            m_aircrafts[row]->m_gui->targetAircraft(m_aircrafts[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    return true;
}

void AircraftModel::findOnMap(int index)
{
    if ((index < 0) || (index >= m_aircrafts.count())) {
        return;
    }
    FeatureWebAPIUtils::mapFind(m_aircrafts[index]->m_icaoHex);
}

QVariant AirportModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_airports.count()))
        return QVariant();
    if (role == AirportModel::positionRole)
    {
        // Coordinates to display the airport icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_airports[row]->m_latitude);
        coords.setLongitude(m_airports[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_airports[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == AirportModel::airportDataRole)
    {
        if (m_showFreq[row])
        {
            QString text = m_airportDataFreq[row];
            if (!m_metar[row].isEmpty()) {
                text = text + "\n" + m_metar[row];
            }
            return QVariant::fromValue(text);
        }
        else
            return QVariant::fromValue(m_airports[row]->m_ident);
    }
    else if (role == AirportModel::airportDataRowsRole)
    {
        if (m_showFreq[row])
        {
            int rows = m_airportDataFreqRows[row];
            if (!m_metar[row].isEmpty()) {
                rows += 1 + m_metar[row].count("\n");
            }
            return QVariant::fromValue(rows);
        }
        else
            return 1;
    }
    else if (role == AirportModel::airportImageRole)
    {
        // Select an image to use for the airport
        if (m_airports[row]->m_type == ADSBDemodSettings::AirportType::Large)
            return QVariant::fromValue(QString("airport_large.png"));
        else if (m_airports[row]->m_type == ADSBDemodSettings::AirportType::Medium)
            return QVariant::fromValue(QString("airport_medium.png"));
        else if (m_airports[row]->m_type == ADSBDemodSettings::AirportType::Heliport)
            return QVariant::fromValue(QString("heliport.png"));
        else
            return QVariant::fromValue(QString("airport_small.png"));
    }
    else if (role == AirportModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the airport
        return QVariant::fromValue(QColor("lightyellow"));
    }
    else if (role == AirportModel::showFreqRole)
    {
        return QVariant::fromValue(m_showFreq[row]);
    }
    return QVariant();
}

bool AirportModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_airports.count()))
        return false;
    if (role == AirportModel::showFreqRole)
    {
        bool showFreq = value.toBool();
        if (showFreq != m_showFreq[row])
        {
            m_showFreq[row] = showFreq;
            emit dataChanged(index, index);
            if (showFreq) {
                emit requestMetar(m_airports[row]->m_ident);
            }
        }
        return true;
    }
    else if (role == AirportModel::selectedFreqRole)
    {
        int idx = value.toInt();
        if ((idx >= 0) && (idx < m_airports[row]->m_frequencies.size()))
            m_gui->setFrequency(m_airports[row]->m_frequencies[idx]->m_frequency * 1000000);
        else if (idx == m_airports[row]->m_frequencies.size())
        {
            // Set airport as target
            m_gui->target(m_airports[row]->m_name, m_azimuth[row], m_elevation[row], m_range[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    return true;
}

QVariant AirspaceModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_airspaces.count())) {
        return QVariant();
    }
    if (role == AirspaceModel::nameRole)
    {
        // Airspace name
        return QVariant::fromValue(m_airspaces[row]->m_name);
    }
    else if (role == AirspaceModel::detailsRole)
    {
        // Airspace name and altitudes
        QString details;
        details.append(m_airspaces[row]->m_name);
        details.append(QString("\n%1 - %2")
                    .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_bottom))
                    .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_top)));
        return QVariant::fromValue(details);
    }
    else if (role == AirspaceModel::positionRole)
    {
        // Coordinates to display the airspace name at
        QGeoCoordinate coords;
        coords.setLatitude(m_airspaces[row]->m_position.y());
        coords.setLongitude(m_airspaces[row]->m_position.x());
        coords.setAltitude(m_airspaces[row]->topHeightInMetres());
        return QVariant::fromValue(coords);
    }
    else if (role == AirspaceModel::airspaceBorderColorRole)
    {
        if (m_airspaces[row]->m_category == "D")
        {
            return QVariant::fromValue(QColor("blue"));
        }
        else
        {
            return QVariant::fromValue(QColor("red"));
        }
    }
    else if (role == AirspaceModel::airspaceFillColorRole)
    {
        if (m_airspaces[row]->m_category == "D")
        {
            return QVariant::fromValue(QColor(0x00, 0x00, 0xff, 0x10));
        }
        else
        {
            return QVariant::fromValue(QColor(0xff, 0x00, 0x00, 0x10));
        }
    }
    else if (role == AirspaceModel::airspacePolygonRole)
    {
       return m_polygons[row];
    }
    return QVariant();
}

bool AirspaceModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    (void) value;
    (void) role;

    int row = index.row();
    if ((row < 0) || (row >= m_airspaces.count())) {
        return false;
    }
    return true;
}

QVariant NavAidModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_navAids.count())) {
        return QVariant();
    }
    if (role == NavAidModel::positionRole)
    {
        // Coordinates to display the VOR icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_navAids[row]->m_latitude);
        coords.setLongitude(m_navAids[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_navAids[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == NavAidModel::navAidDataRole)
    {
        // Create the text to go in the bubble next to the VOR
        if (m_selected[row])
        {
            QStringList list;
            list.append(QString("Name: %1").arg(m_navAids[row]->m_name));
            if (m_navAids[row]->m_type == "NDB") {
                list.append(QString("Frequency: %1 kHz").arg(m_navAids[row]->m_frequencykHz, 0, 'f', 1));
            } else {
                list.append(QString("Frequency: %1 MHz").arg(m_navAids[row]->m_frequencykHz / 1000.0f, 0, 'f', 2));
            }
            if (m_navAids[row]->m_channel != "") {
                list.append(QString("Channel: %1").arg(m_navAids[row]->m_channel));
            }
            list.append(QString("Ident: %1 %2").arg(m_navAids[row]->m_ident).arg(Morse::toSpacedUnicodeMorse(m_navAids[row]->m_ident)));
            list.append(QString("Range: %1 nm").arg(m_navAids[row]->m_range));
            if (m_navAids[row]->m_alignedTrueNorth) {
                list.append(QString("Magnetic declination: Aligned to true North"));
            } else if (m_navAids[row]->m_magneticDeclination != 0.0f) {
                list.append(QString("Magnetic declination: %1%2").arg(std::round(m_navAids[row]->m_magneticDeclination)).arg(QChar(0x00b0)));
            }
            QString data = list.join("\n");
            return QVariant::fromValue(data);
        }
        else
        {
            return QVariant::fromValue(m_navAids[row]->m_name);
        }
    }
    else if (role == NavAidModel::navAidImageRole)
    {
        // Select an image to use for the NavAid
        return QVariant::fromValue(QString("%1.png").arg(m_navAids[row]->m_type));
    }
    else if (role == NavAidModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the NavAid
        return QVariant::fromValue(QColor("lightgreen"));
    }
    else if (role == NavAidModel::selectedRole)
    {
        return QVariant::fromValue(m_selected[row]);
    }
    return QVariant();
}

bool NavAidModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_navAids.count())) {
        return false;
    }
    if (role == NavAidModel::selectedRole)
    {
        bool selected = value.toBool();
        m_selected[row] = selected;
        emit dataChanged(index, index);
        return true;
    }
    return true;
}

// Set selected device to the given centre frequency (used to tune to ATC selected from airports on map)
bool ADSBDemodGUI::setFrequency(float targetFrequencyHz)
{
    return ChannelWebAPIUtils::setCenterFrequency(m_settings.m_deviceIndex, targetFrequencyHz);
}

// Called when we have both lat & long
void ADSBDemodGUI::updatePosition(Aircraft *aircraft)
{
    if (!aircraft->m_positionValid)
    {
        aircraft->m_positionValid = true;
        // Now we have a position, add a plane to the map
        QGeoCoordinate coords;
        coords.setLatitude(aircraft->m_latitude);
        coords.setLongitude(aircraft->m_longitude);
        m_aircraftModel.addAircraft(aircraft);
    }
    // Calculate range, azimuth and elevation to aircraft from station
    m_azEl.setTarget(aircraft->m_latitude, aircraft->m_longitude, Units::feetToMetres(aircraft->m_altitude));
    m_azEl.calculate();
    aircraft->m_range = m_azEl.getDistance();
    aircraft->m_azimuth = m_azEl.getAzimuth();
    aircraft->m_elevation = m_azEl.getElevation();
    aircraft->m_rangeItem->setText(QString::number(aircraft->m_range/1000.0, 'f', 1));
    aircraft->m_azElItem->setText(QString("%1/%2").arg(std::round(aircraft->m_azimuth)).arg(std::round(aircraft->m_elevation)));
    if (aircraft == m_trackAircraft) {
        m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
    }
}

// Called when we have lat & long from local decode and we need to check if it is in a valid range (<180nm/333km)
// or 1/4 of that for surface positions
bool ADSBDemodGUI::updateLocalPosition(Aircraft *aircraft, double latitude, double longitude, bool surfacePosition)
{
    // Calculate range to aircraft from station
    m_azEl.setTarget(latitude, longitude, Units::feetToMetres(aircraft->m_altitude));
    m_azEl.calculate();

    // Don't use the full 333km, as there may be some error in station position
    if (m_azEl.getDistance() < (surfacePosition ? 80000 : 320000))
    {
        aircraft->m_latitude = latitude;
        aircraft->m_longitude = longitude;
        updatePosition(aircraft);
        return true;
    }
    else
    {
        //qDebug() << "Local position out of range - calculated distance: " << m_azEl.getDistance();
        return false;
    }
}

void ADSBDemodGUI::sendToMap(Aircraft *aircraft, QList<SWGSDRangel::SWGMapAnimation *> *animations)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_adsbDemod, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        // Adjust altitude by airfield barometric elevation, so aircraft appears to
        // take-off/land at correct point on runway
        int altitudeFt = aircraft->m_altitude;

        if (!aircraft->m_onSurface && !aircraft->m_altitudeGNSS) {
            altitudeFt -= m_settings.m_airfieldElevation;
        }

        float altitudeM = Units::feetToMetres(altitudeFt);

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(aircraft->m_icaoHex));
            swgMapItem->setLatitude(aircraft->m_latitude);
            swgMapItem->setLongitude(aircraft->m_longitude);
            swgMapItem->setAltitude(altitudeM);
            swgMapItem->setPositionDateTime(new QString(aircraft->m_positionDateTime.toString(Qt::ISODateWithMs)));
            swgMapItem->setFixedPosition(false);
            swgMapItem->setImage(new QString(QString("qrc:///map/%1").arg(aircraft->getImage())));
            swgMapItem->setImageRotation(aircraft->m_heading);
            swgMapItem->setText(new QString(aircraft->getText(true)));

            if (!aircraft->m_aircraft3DModel.isEmpty()) {
                swgMapItem->setModel(new QString(aircraft->m_aircraft3DModel));
            } else {
                swgMapItem->setModel(new QString(aircraft->m_aircraftCat3DModel));
            }

            swgMapItem->setLabel(new QString(aircraft->m_callsign));

            if (aircraft->m_headingValid)
            {
                swgMapItem->setOrientation(1);
                swgMapItem->setHeading(aircraft->m_heading);
                swgMapItem->setPitch(aircraft->m_pitch);
                swgMapItem->setRoll(aircraft->m_roll);
                swgMapItem->setOrientationDateTime(new QString(aircraft->m_positionDateTime.toString(Qt::ISODateWithMs)));
            }
            else
            {
                // Orient aircraft based on velocity calculated from position
                swgMapItem->setOrientation(0);
            }

            swgMapItem->setModelAltitudeOffset(aircraft->m_modelAltitudeOffset);
            swgMapItem->setLabelAltitudeOffset(aircraft->m_labelAltitudeOffset);
            swgMapItem->setAltitudeReference(3); // CLIP_TO_GROUND so aircraft don't go under runway
            swgMapItem->setAnimations(animations);  // Does this need to be duplicated?

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_adsbDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

QString ADSBDemodGUI::getAirlineIconPath(const QString &operatorICAO)
{
    QString endPath = QString("/airlinelogos/%1.bmp").arg(operatorICAO);
    // Try in user directory first, so they can customise
    QString userIconPath = getDataDir() + endPath;
    QFile file(userIconPath);
    if (file.exists())
    {
        return userIconPath;
    }
    else
    {
        // Try in resources
        QString resourceIconPath = ":" + endPath;
        QResource resource(resourceIconPath);
        if (resource.isValid())
        {
            return resourceIconPath;
        }
    }
    return QString();
}

// Try to find an airline logo based on ICAO
QIcon *ADSBDemodGUI::getAirlineIcon(const QString &operatorICAO)
{
    if (m_airlineIcons.contains(operatorICAO))
    {
        return m_airlineIcons.value(operatorICAO);
    }
    else
    {
        QIcon *icon = nullptr;
        QString path = getAirlineIconPath(operatorICAO);
        if (!path.isEmpty())
        {
            icon = new QIcon(path);
            m_airlineIcons.insert(operatorICAO, icon);
        }
        else
        {
            if (!m_airlineMissingIcons.contains(operatorICAO))
            {
                qDebug() << "ADSBDemodGUI: No airline logo for " << operatorICAO;
                m_airlineMissingIcons.insert(operatorICAO, true);
            }
        }
        return icon;
    }
}

QString ADSBDemodGUI::getFlagIconPath(const QString &country)
{
    QString endPath = QString("/flags/%1.bmp").arg(country);
    // Try in user directory first, so they can customise
    QString userIconPath = getDataDir() + endPath;
    QFile file(userIconPath);
    if (file.exists())
    {
        return userIconPath;
    }
    else
    {
        // Try in resources
        QString resourceIconPath = ":" + endPath;
        QResource resource(resourceIconPath);
        if (resource.isValid())
        {
            return resourceIconPath;
        }
    }
    return QString();
}

// Try to find an flag logo based on a country
QIcon *ADSBDemodGUI::getFlagIcon(const QString &country)
{
    if (m_flagIcons.contains(country))
    {
        return m_flagIcons.value(country);
    }
    else
    {
        QIcon *icon = nullptr;
        QString path = getFlagIconPath(country);
        if (!path.isEmpty())
        {
            icon = new QIcon(path);
            m_flagIcons.insert(country, icon);
        }
        return icon;
    }
}

// Find aircraft with icao, or create if it doesn't exist
Aircraft *ADSBDemodGUI::getAircraft(int icao, bool &newAircraft)
{
    Aircraft *aircraft;

    if (m_aircraft.contains(icao))
    {
        // Update existing aircraft info
        aircraft = m_aircraft.value(icao);
    }
    else
    {
        // Add new aircraft
        newAircraft = true;
        aircraft = new Aircraft(this);
        aircraft->m_icao = icao;
        aircraft->m_icaoHex = QString::number(aircraft->m_icao, 16);
        m_aircraft.insert(icao, aircraft);
        aircraft->m_icaoItem->setText(aircraft->m_icaoHex);
        ui->adsbData->setSortingEnabled(false);
        int row = ui->adsbData->rowCount();
        ui->adsbData->setRowCount(row + 1);
        ui->adsbData->setItem(row, ADSB_COL_ICAO, aircraft->m_icaoItem);
        ui->adsbData->setItem(row, ADSB_COL_CALLSIGN, aircraft->m_callsignItem);
        ui->adsbData->setItem(row, ADSB_COL_MODEL, aircraft->m_modelItem);
        ui->adsbData->setItem(row, ADSB_COL_AIRLINE, aircraft->m_airlineItem);
        ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, aircraft->m_altitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_SPEED, aircraft->m_speedItem);
        ui->adsbData->setItem(row, ADSB_COL_HEADING, aircraft->m_headingItem);
        ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, aircraft->m_verticalRateItem);
        ui->adsbData->setItem(row, ADSB_COL_RANGE, aircraft->m_rangeItem);
        ui->adsbData->setItem(row, ADSB_COL_AZEL, aircraft->m_azElItem);
        ui->adsbData->setItem(row, ADSB_COL_LATITUDE, aircraft->m_latitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, aircraft->m_longitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_CATEGORY, aircraft->m_emitterCategoryItem);
        ui->adsbData->setItem(row, ADSB_COL_STATUS, aircraft->m_statusItem);
        ui->adsbData->setItem(row, ADSB_COL_SQUAWK, aircraft->m_squawkItem);
        ui->adsbData->setItem(row, ADSB_COL_REGISTRATION, aircraft->m_registrationItem);
        ui->adsbData->setItem(row, ADSB_COL_COUNTRY, aircraft->m_countryItem);
        ui->adsbData->setItem(row, ADSB_COL_REGISTERED, aircraft->m_registeredItem);
        ui->adsbData->setItem(row, ADSB_COL_MANUFACTURER, aircraft->m_manufacturerNameItem);
        ui->adsbData->setItem(row, ADSB_COL_OWNER, aircraft->m_ownerItem);
        ui->adsbData->setItem(row, ADSB_COL_OPERATOR_ICAO, aircraft->m_operatorICAOItem);
        ui->adsbData->setItem(row, ADSB_COL_TIME, aircraft->m_timeItem);
        ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, aircraft->m_adsbFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_CORRELATION, aircraft->m_correlationItem);
        ui->adsbData->setItem(row, ADSB_COL_RSSI, aircraft->m_rssiItem);
        ui->adsbData->setItem(row, ADSB_COL_FLIGHT_STATUS, aircraft->m_flightStatusItem);
        ui->adsbData->setItem(row, ADSB_COL_DEP, aircraft->m_depItem);
        ui->adsbData->setItem(row, ADSB_COL_ARR, aircraft->m_arrItem);
        ui->adsbData->setItem(row, ADSB_COL_STD, aircraft->m_stdItem);
        ui->adsbData->setItem(row, ADSB_COL_ETD, aircraft->m_etdItem);
        ui->adsbData->setItem(row, ADSB_COL_ATD, aircraft->m_atdItem);
        ui->adsbData->setItem(row, ADSB_COL_STA, aircraft->m_staItem);
        ui->adsbData->setItem(row, ADSB_COL_ETA, aircraft->m_etaItem);
        ui->adsbData->setItem(row, ADSB_COL_ATA, aircraft->m_ataItem);
        // Look aircraft up in database
        if (m_aircraftInfo != nullptr)
        {
            if (m_aircraftInfo->contains(icao))
            {
                aircraft->m_aircraftInfo = m_aircraftInfo->value(icao);
                aircraft->m_modelItem->setText(aircraft->m_aircraftInfo->m_model);
                aircraft->m_registrationItem->setText(aircraft->m_aircraftInfo->m_registration);
                aircraft->m_manufacturerNameItem->setText(aircraft->m_aircraftInfo->m_manufacturerName);
                aircraft->m_ownerItem->setText(aircraft->m_aircraftInfo->m_owner);
                aircraft->m_operatorICAOItem->setText(aircraft->m_aircraftInfo->m_operatorICAO);
                aircraft->m_registeredItem->setText(aircraft->m_aircraftInfo->m_registered);
                // Try loading an airline logo based on operator ICAO
                QIcon *icon = nullptr;
                if (aircraft->m_aircraftInfo->m_operatorICAO.size() > 0)
                {
                    aircraft->m_airlineIconURL = getAirlineIconPath(aircraft->m_aircraftInfo->m_operatorICAO);
                    if (aircraft->m_airlineIconURL.startsWith(':')) {
                        aircraft->m_airlineIconURL = "qrc://" + aircraft->m_airlineIconURL.mid(1);
                    }
                    icon = getAirlineIcon(aircraft->m_aircraftInfo->m_operatorICAO);
                    if (icon != nullptr)
                    {
                        aircraft->m_airlineItem->setSizeHint(QSize(85, 20));
                        aircraft->m_airlineItem->setIcon(*icon);
                    }
                }
                if (icon == nullptr)
                {
                    if (aircraft->m_aircraftInfo->m_operator.size() > 0)
                        aircraft->m_airlineItem->setText(aircraft->m_aircraftInfo->m_operator);
                    else
                        aircraft->m_airlineItem->setText(aircraft->m_aircraftInfo->m_owner);
                }
                // Try loading a flag based on registration
                if ((aircraft->m_aircraftInfo->m_registration.size() > 0) && (m_prefixMap != nullptr))
                {
                    QString flag;
                    int idx = aircraft->m_aircraftInfo->m_registration.indexOf('-');
                    if (idx >= 0)
                    {
                        QString prefix;

                        // Some countries use AA-A - try these first as first letters are common
                        prefix = aircraft->m_aircraftInfo->m_registration.left(idx + 2);
                        if (m_prefixMap->contains(prefix))
                            flag = m_prefixMap->value(prefix);
                        else
                        {
                            // Try letters before '-'
                            prefix = aircraft->m_aircraftInfo->m_registration.left(idx);
                            if (m_prefixMap->contains(prefix))
                                flag = m_prefixMap->value(prefix);
                        }
                    }
                    else
                    {
                        // No '-' Could be one of a few countries or military.
                        // See: https://en.wikipedia.org/wiki/List_of_aircraft_registration_prefixes
                        if (aircraft->m_aircraftInfo->m_registration.startsWith("N")) {
                            flag = m_prefixMap->value("N"); // US
                        } else if (aircraft->m_aircraftInfo->m_registration.startsWith("JA")) {
                            flag = m_prefixMap->value("JA"); // Japan
                        } else if (aircraft->m_aircraftInfo->m_registration.startsWith("HL")) {
                            flag = m_prefixMap->value("HL"); // Korea
                        } else if (aircraft->m_aircraftInfo->m_registration.startsWith("YV")) {
                            flag = m_prefixMap->value("YV"); // Venezuela
                        } else if ((m_militaryMap != nullptr) && (m_militaryMap->contains(aircraft->m_aircraftInfo->m_operator))) {
                            flag = m_militaryMap->value(aircraft->m_aircraftInfo->m_operator);
                        }
                    }
                    if (flag != "")
                    {
                        aircraft->m_flagIconURL = getFlagIconPath(flag);
                        if (aircraft->m_flagIconURL.startsWith(':')) {
                            aircraft->m_flagIconURL = "qrc://" + aircraft->m_flagIconURL.mid(1);
                        }
                        icon = getFlagIcon(flag);
                        if (icon != nullptr)
                        {
                            aircraft->m_countryItem->setSizeHint(QSize(40, 20));
                            aircraft->m_countryItem->setIcon(*icon);
                        }
                    }
                }
                get3DModel(aircraft);
            }
        }

        if (aircraft->m_aircraft3DModel.isEmpty())
        {
            // Default to A320 until we get some more info
            aircraft->m_aircraftCat3DModel = get3DModel("A320");
            if (m_modelAltitudeOffset.contains("A320"))
            {
                aircraft->m_modelAltitudeOffset = m_modelAltitudeOffset.value("A320");
                aircraft->m_labelAltitudeOffset = m_labelAltitudeOffset.value("A320");
            }
        }

        if (m_settings.m_autoResizeTableColumns)
            ui->adsbData->resizeColumnsToContents();
        ui->adsbData->setSortingEnabled(true);
        // Check to see if we need to emit a notification about this new aircraft
        checkStaticNotification(aircraft);
    }

    return aircraft;
}

void ADSBDemodGUI::handleADSB(
    const QByteArray data,
    const QDateTime dateTime,
    float correlation,
    float correlationOnes,
    bool updateModel)
{
    const char idMap[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ############-##0123456789######";
    const QString categorySetA[] = {
        QStringLiteral("None"),
        QStringLiteral("Light"),
        QStringLiteral("Small"),
        QStringLiteral("Large"),
        QStringLiteral("High vortex"),
        QStringLiteral("Heavy"),
        QStringLiteral("High performance"),
        QStringLiteral("Rotorcraft")
    };
    const QString categorySetB[] = {
        QStringLiteral("None"),
        QStringLiteral("Glider/sailplane"),
        QStringLiteral("Lighter-than-air"),
        QStringLiteral("Parachutist"),
        QStringLiteral("Ultralight"),
        QStringLiteral("Reserved"),
        QStringLiteral("UAV"),
        QStringLiteral("Space vehicle")
    };
    const QString categorySetC[] = {
        QStringLiteral("None"),
        QStringLiteral("Emergency vehicle"),
        QStringLiteral("Service vehicle"),
        QStringLiteral("Ground obstruction"),
        QStringLiteral("Cluster obstacle"),
        QStringLiteral("Line obstacle"),
        QStringLiteral("Reserved"),
        QStringLiteral("Reserved")
    };
    const QString emergencyStatus[] = {
        QStringLiteral("No emergency"),
        QStringLiteral("General emergency"),
        QStringLiteral("Lifeguard/Medical"),
        QStringLiteral("Minimum fuel"),
        QStringLiteral("No communications"),
        QStringLiteral("Unlawful interference"),
        QStringLiteral("Downed aircraft"),
        QStringLiteral("Reserved")
    };

    bool newAircraft = false;
    bool updatedCallsign = false;
    bool resetAnimation = false;

    int df = (data[0] >> 3) & ADS_B_DF_MASK; // Downlink format
    int ca = data[0] & 0x7; // Capability
    unsigned icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff); // ICAO aircraft address
    int tc = (data[4] >> 3) & 0x1f; // Type code

    Aircraft *aircraft = getAircraft(icao, newAircraft);

    aircraft->m_time = dateTime;
    QTime time = dateTime.time();
    aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
    aircraft->m_adsbFrameCount++;
    aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);

    if (correlation < aircraft->m_minCorrelation)
        aircraft->m_minCorrelation = correlation;
    if (correlation > aircraft->m_maxCorrelation)
        aircraft->m_maxCorrelation = correlation;
    m_correlationAvg(correlation);
    aircraft->m_correlationAvg(correlation);
    aircraft->m_correlation = aircraft->m_correlationAvg.instantAverage();
    aircraft->m_correlationItem->setText(QString("%1/%2/%3")
        .arg(CalcDb::dbPower(aircraft->m_minCorrelation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_correlation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_maxCorrelation), 3, 'f', 1));
    m_correlationOnesAvg(correlationOnes);
    aircraft->m_rssiItem->setText(QString("%1")
        .arg(CalcDb::dbPower(m_correlationOnesAvg.instantAverage()), 3, 'f', 1));

    // ADS-B, non-transponder ADS-B or TIS-B rebroadcast of ADS-B (ADS-R)
    if ((df == 17) || ((df == 18) && ((ca == 0) || (ca == 1) || (ca == 6))))
    {
        if ((tc >= 1) && ((tc <= 4)))
        {
            // Aircraft identification
            int ec = data[4] & 0x7;   // Emitter category

            QString prevEmitterCategory = aircraft->m_emitterCategory;
            if (tc == 4) {
                aircraft->m_emitterCategory = categorySetA[ec];
            } else if (tc == 3) {
                aircraft->m_emitterCategory = categorySetB[ec];
            } else if (tc == 2) {
                aircraft->m_emitterCategory = categorySetC[ec];
            } else {
                aircraft->m_emitterCategory = QStringLiteral("Reserved");
            }
            aircraft->m_emitterCategoryItem->setText(aircraft->m_emitterCategory);

            // Flight/callsign - Extract 8 6-bit characters from 6 8-bit bytes, MSB first
            unsigned char c[8];
            char callsign[9];
            c[0] = (data[5] >> 2) & 0x3f; // 6
            c[1] = ((data[5] & 0x3) << 4) | ((data[6] & 0xf0) >> 4);  // 2+4
            c[2] = ((data[6] & 0xf) << 2) | ((data[7] & 0xc0) >> 6);  // 4+2
            c[3] = (data[7] & 0x3f); // 6
            c[4] = (data[8] >> 2) & 0x3f;
            c[5] = ((data[8] & 0x3) << 4) | ((data[9] & 0xf0) >> 4);
            c[6] = ((data[9] & 0xf) << 2) | ((data[10] & 0xc0) >> 6);
            c[7] = (data[10] & 0x3f);
            // Map to ASCII
            for (int i = 0; i < 8; i++)
                callsign[i] = idMap[c[i]];
            callsign[8] = '\0';
            QString callsignTrimmed = QString(callsign).trimmed();

            updatedCallsign = aircraft->m_callsign != callsignTrimmed;

            aircraft->m_callsign = callsignTrimmed;
            aircraft->m_callsignItem->setText(aircraft->m_callsign);

            // Attempt to map callsign to flight number
            if (!aircraft->m_callsign.isEmpty())
            {
                QRegularExpression flightNoExp("^[A-Z]{2,3}[0-9]{1,4}$");
                // Airlines line BA add a single charater suffix that can be stripped
                // If the suffix is two characters, then it typically means a digit
                // has been replaced which I don't know how to guess
                // E.g Easyjet might use callsign EZY67JQ for flight EZY6267
                // BA use BAW90BG for BA890
                QRegularExpression suffixedFlightNoExp("^([A-Z]{2,3})([0-9]{1,4})[A-Z]?$");
                QRegularExpressionMatch suffixMatch;

                if (flightNoExp.match(aircraft->m_callsign).hasMatch())
                {
                    aircraft->m_flight = aircraft->m_callsign;
                }
                else if ((suffixMatch = suffixedFlightNoExp.match(aircraft->m_callsign)).hasMatch())
                {
                    aircraft->m_flight = QString("%1%2").arg(suffixMatch.captured(1)).arg(suffixMatch.captured(2));
                }
                else
                {
                    // Don't guess, to save wasting API calls
                    aircraft->m_flight = "";
                }
            }
            else
            {
                aircraft->m_flight = "";
            }

            // Select 3D model based on category, if we don't already have one based on ICAO
            if (   aircraft->m_aircraft3DModel.isEmpty()
                && (   aircraft->m_aircraftCat3DModel.isEmpty()
                    || (prevEmitterCategory != aircraft->m_emitterCategory)
                   )
               )
            {
                get3DModelBasedOnCategory(aircraft);
                // As we're changing the model, we need to reset animations to
                // ensure gear/flaps are in correct position on new model
                resetAnimation = true;
            }
        }
        else if (((tc >= 5) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)))
        {
            bool wasOnSurface = aircraft->m_onSurface;
            aircraft->m_onSurface = (tc >= 5) && (tc <= 8);

            if (wasOnSurface != aircraft->m_onSurface)
            {
                // Can't mix CPR values used on surface and those that are airbourne
                aircraft->m_cprValid[0] = false;
                aircraft->m_cprValid[1] = false;
            }

            if (aircraft->m_onSurface)
            {
                // Surface position

                // There are a few airports that are below 0 MSL
                // https://en.wikipedia.org/wiki/List_of_lowest_airports
                // So we set altitude to a negative value here, which should
                // then get clipped to actual terrain elevation in 3D map
                aircraft->m_altitudeValid = true;
                aircraft->m_altitude = -200;
                aircraft->m_altitudeItem->setData(Qt::DisplayRole, "Surface");

                int movement = ((data[4] & 0x7) << 4) | ((data[5] >> 4) & 0xf);
                if (movement == 0)
                {
                    // No information available
                    aircraft->m_speedValid = false;
                    aircraft->m_speedItem->setData(Qt::DisplayRole, "");
                }
                else if (movement == 1)
                {
                    // Aircraft stopped
                    aircraft->m_speedValid = true;
                    aircraft->m_speedItem->setData(Qt::DisplayRole, 0);
                    aircraft->m_speed = 0.0;
                }
                else if ((movement >= 2) && (movement <= 123))
                {
                    float base, step; // In knts
                    int adjust;
                    if ((movement >= 2) && (movement <= 8))
                    {
                        base = 0.125f;
                        step = 0.125f;
                        adjust = 2;
                    }
                    else if ((movement >= 9) && (movement <= 12))
                    {
                        base = 1.0f;
                        step = 0.25f;
                        adjust = 9;
                    }
                    else if ((movement >= 13) && (movement <= 38))
                    {
                        base = 2.0f;
                        step = 0.5f;
                        adjust = 13;
                    }
                    else if ((movement >= 39) && (movement <= 93))
                    {
                        base = 15.0f;
                        step = 1.0f;
                        adjust = 39;
                    }
                    else if ((movement >= 94) && (movement <= 108))
                    {
                        base = 70.0f;
                        step = 2.0f;
                        adjust = 94;
                    }
                    else
                    {
                        base = 100.0f;
                        step = 5.0f;
                        adjust = 109;
                    }
                    aircraft->m_speed = base + (movement - adjust) * step;
                    aircraft->m_speedType = Aircraft::GS;
                    aircraft->m_speedValid = true;
                    aircraft->m_speedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_speed) : (int)std::round(aircraft->m_speed));
                }
                else if (movement == 124)
                {
                    aircraft->m_speedValid = true;
                    aircraft->m_speedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? 324 : 175); // Actually greater than this
                }

                int groundTrackStatus = (data[5] >> 3) & 1;
                int groundTrackValue = ((data[5] & 0x7) << 4) | ((data[6] >> 4) & 0xf);
                if (groundTrackStatus)
                {
                    aircraft->m_heading = groundTrackValue * 360.0/128.0;
                    aircraft->m_headingValid = true;
                    aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                }
            }
            else if (((tc >= 9) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)))
            {
                // Airbourne position (9-18 baro, 20-22 GNSS)
                int alt = ((data[5] & 0xff) << 4) | ((data[6] >> 4) & 0xf); // Altitude
                int q = (alt & 0x10) != 0;
                int n = ((alt >> 1) & 0x7f0) | (alt & 0xf);  // Remove Q-bit
                int alt_ft;
                if (q == 1)
                {
                    alt_ft = n * ((alt & 0x10) ? 25 : 100) - 1000;
                }
                else
                {
                    // https://en.wikipedia.org/wiki/Gillham_code
                    int c1 = (n >> 10) & 1;
                    int a1 = (n >> 9) & 1;
                    int c2 = (n >> 8) & 1;
                    int a2 = (n >> 7) & 1;
                    int c4 = (n >> 6) & 1;
                    int a4 = (n >> 5) & 1;
                    int b1 = (n >> 4) & 1;
                    int b2 = (n >> 3) & 1;
                    int d2 = (n >> 2) & 1;
                    int b4 = (n >> 1) & 1;
                    int d4 = n & 1;

                    int n500 = grayToBinary((d2 << 7) | (d4 << 6) | (a1 << 5) | (a2 << 4) | (a4 << 3) | (b1 << 2) | (b2 << 1) | b4, 4);
                    int n100 = grayToBinary((c1 << 2) | (c2 << 1) | c4, 3) - 1;

                    if (n100 == 6) {
                        n100 = 4;
                    }
                    if (n500 %2 != 0) {
                        n100 = 4 - n100;
                    }

                    alt_ft = -1200 + n500*500 + n100*100;
                }

                aircraft->m_altitude = alt_ft;
                aircraft->m_altitudeValid = alt != 0;
                aircraft->m_altitudeGNSS = ((tc >= 20) && (tc <= 22));
                // setData rather than setText so it sorts numerically
                aircraft->m_altitudeItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::feetToIntegerMetres(aircraft->m_altitude) : aircraft->m_altitude);

                // Assume runway elevation is at first reported airboune altitude
                if (wasOnSurface)
                {
                    aircraft->m_runwayAltitude = aircraft->m_altitude;
                    aircraft->m_runwayAltitudeValid = true;
                }
            }

            int f = (data[6] >> 2) & 1; // CPR odd/even frame - should alternate every 0.2s
            int lat_cpr = ((data[6] & 3) << 15) | ((data[7] & 0xff) << 7) | ((data[8] >> 1) & 0x7f);
            int lon_cpr = ((data[8] & 1) << 16) | ((data[9] & 0xff) << 8) | (data[10] & 0xff);

            aircraft->m_cprValid[f] = true;
            aircraft->m_cprLat[f] = lat_cpr/131072.0f;
            aircraft->m_cprLong[f] = lon_cpr/131072.0f;
            aircraft->m_cprTime[f] = dateTime;

            // CPR decoding
            // Refer to Technical Provisions  for Mode S Services and Extended Squitter - Appendix C2.6
            // See also: https://mode-s.org/decode/adsb/airborne-position.html
            // For global decoding, we need both odd and even frames
            // We also need to check that both frames aren't greater than 10s apart in time (C.2.6.7), otherwise position may be out by ~10deg
            // I've reduced this to 8.5s, as problems have been seen where times are just 9s apart. This may be because
            // our timestamps aren't accurate, as the times are generated when packets are decoded on buffered data.
            // We could compare global + local methods to see if the positions are sensible
            if (aircraft->m_cprValid[0] && aircraft->m_cprValid[1]
               && (std::abs(aircraft->m_cprTime[0].toMSecsSinceEpoch() - aircraft->m_cprTime[1].toMSecsSinceEpoch()) <= 8500)
               && !aircraft->m_onSurface)
            {
                // Global decode using odd and even frames (C.2.6)

                // Calculate latitude
                const double dLatEven = 360.0/60.0;
                const double dLatOdd = 360.0/59.0;
                double latEven, latOdd;
                double latitude, longitude;
                int ni, m;

                int j = std::floor(59.0f*aircraft->m_cprLat[0] - 60.0f*aircraft->m_cprLat[1] + 0.5);
                latEven = dLatEven * (modulus(j, 60) + aircraft->m_cprLat[0]);
                // Southern hemisphere is in range 270-360, so adjust to -90-0
                if (latEven >= 270.0f)
                    latEven -= 360.0f;
                latOdd = dLatOdd * (modulus(j, 59) + aircraft->m_cprLat[1]);
                if (latOdd >= 270.0f)
                    latOdd -= 360.0f;
                if (aircraft->m_cprTime[0] >= aircraft->m_cprTime[1])
                    latitude = latEven;
                else
                    latitude = latOdd;
                if ((latitude <= 90.0) && (latitude >= -90.0))
                {
                    // Check if both frames in same latitude zone
                    int latEvenNL = cprNL(latEven);
                    int latOddNL = cprNL(latOdd);
                    if (latEvenNL == latOddNL)
                    {
                        // Calculate longitude
                        if (!f)
                        {
                            ni = cprN(latEven, 0);
                            m = std::floor(aircraft->m_cprLong[0] * (latEvenNL - 1) - aircraft->m_cprLong[1] * latEvenNL + 0.5f);
                            longitude = (360.0f/ni) * (modulus(m, ni) + aircraft->m_cprLong[0]);
                        }
                        else
                        {
                            ni = cprN(latOdd, 1);
                            m = std::floor(aircraft->m_cprLong[0] * (latOddNL - 1) - aircraft->m_cprLong[1] * latOddNL + 0.5f);
                            longitude = (360.0f/ni) * (modulus(m, ni) + aircraft->m_cprLong[1]);
                        }
                        if (longitude > 180.0f)
                            longitude -= 360.0f;
                        aircraft->m_latitude = latitude;
                        aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
                        aircraft->m_longitude = longitude;
                        aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                        aircraft->m_positionDateTime = dateTime;
                        QGeoCoordinate coord(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude);
                        aircraft->m_coordinates.push_back(QVariant::fromValue(coord));
                        updatePosition(aircraft);
                    }
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::handleADSB: Invalid latitude " << latitude << " for " << QString("%1").arg(aircraft->m_icaoHex)
                        << " m_cprLat[0] " << aircraft->m_cprLat[0]
                        << " m_cprLat[1] " << aircraft->m_cprLat[1];
                    aircraft->m_cprValid[0] = false;
                    aircraft->m_cprValid[1] = false;
                }
            }
            else
            {
                // Local decode using a single aircraft position + location of receiver
                // Only valid if airbourne within 180nm/333km (C.2.6.4) or 45nm for surface

                // Caclulate latitude
                const double maxDeg = aircraft->m_onSurface ? 90.0 : 360.0;
                const double dLatEven = maxDeg/60.0;
                const double dLatOdd = maxDeg/59.0;
                double dLat = f ? dLatOdd : dLatEven;
                double latitude, longitude;

                int j = std::floor(m_azEl.getLocationSpherical().m_latitude/dLat) + std::floor(modulus(m_azEl.getLocationSpherical().m_latitude, dLat)/dLat - aircraft->m_cprLat[f] + 0.5);
                latitude = dLat * (j + aircraft->m_cprLat[f]);

                // Caclulate longitude
                double dLong;
                int latNL = cprNL(latitude);
                if (f == 0)
                {
                    if (latNL > 0)
                        dLong = maxDeg / latNL;
                    else
                        dLong = maxDeg;
                }
                else
                {
                    if ((latNL - 1) > 0)
                        dLong = maxDeg / (latNL - 1);
                    else
                        dLong = maxDeg;
                }
                int m = std::floor(m_azEl.getLocationSpherical().m_longitude/dLong) + std::floor(modulus(m_azEl.getLocationSpherical().m_longitude, dLong)/dLong - aircraft->m_cprLong[f] + 0.5);
                longitude =  dLong * (m + aircraft->m_cprLong[f]);

                if (updateLocalPosition(aircraft, latitude, longitude, aircraft->m_onSurface))
                {
                    aircraft->m_latitude = latitude;
                    aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
                    aircraft->m_longitude = longitude;
                    aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                    aircraft->m_positionDateTime = dateTime;
                    QGeoCoordinate coord(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude);
                    aircraft->m_coordinates.push_back(QVariant::fromValue(coord));
                }
            }
        }
        else if (tc == 19)
        {
            // Airbourne velocity
            int st = data[4] & 0x7;   // Subtype
            if ((st == 1) || (st == 2))
            {
                // Ground speed
                int s_ew = (data[5] >> 2) & 1; // East-west velocity sign
                int v_ew = ((data[5] & 0x3) << 8) | (data[6] & 0xff); // East-west velocity
                int s_ns = (data[7] >> 7) & 1; // North-south velocity sign
                int v_ns = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // North-south velocity

                int v_we;
                int v_sn;
                float v;
                float h;

                if (s_ew) {
                    v_we = -1 * (v_ew - 1);
                } else {
                    v_we = v_ew - 1;
                }
                if (s_ns) {
                    v_sn = -1 * (v_ns - 1);
                } else {
                    v_sn = v_ns - 1;
                }
                v = std::round(std::sqrt(v_we*v_we + v_sn*v_sn));
                h = std::atan2(v_we, v_sn) * 360.0/(2.0*M_PI);
                if (h < 0.0) {
                    h += 360.0;
                }

                aircraft->m_heading = h;
                aircraft->m_headingValid = true;
                aircraft->m_headingDateTime = dateTime;
                aircraft->m_speed = v;
                aircraft->m_speedType = Aircraft::GS;
                aircraft->m_speedValid = true;
                aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                aircraft->m_speedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_speed) : aircraft->m_speed);
                aircraft->m_orientationDateTime = dateTime;
            }
            else
            {
                // Airspeed
                int s_hdg = (data[5] >> 2) & 1; // Heading status
                int hdg =  ((data[5] & 0x3) << 8) | (data[6] & 0xff); // Heading
                if (s_hdg)
                {
                    aircraft->m_heading = hdg/1024.0f*360.0f;
                    aircraft->m_headingValid = true;
                    aircraft->m_headingDateTime = dateTime;
                    aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                    aircraft->m_orientationDateTime = dateTime;
                }

                int as_t = (data[7] >> 7) & 1; // Airspeed type
                int as = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // Airspeed

                aircraft->m_speed = as;
                aircraft->m_speedType = as_t ? Aircraft::IAS : Aircraft::TAS;
                aircraft->m_speedValid = true;
                aircraft->m_speedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_speed) : aircraft->m_speed);
            }
            int s_vr = (data[8] >> 3) & 1; // Vertical rate sign
            int vr = ((data[8] & 0x7) << 6) | ((data[9] >> 2) & 0x3f); // Vertical rate
            aircraft->m_verticalRate = (vr-1)*64*(s_vr?-1:1);
            aircraft->m_verticalRateValid = true;
            if (m_settings.m_siUnits)
                aircraft->m_verticalRateItem->setData(Qt::DisplayRole, Units::feetPerMinToIntegerMetresPerSecond(aircraft->m_verticalRate));
            else
                aircraft->m_verticalRateItem->setData(Qt::DisplayRole, aircraft->m_verticalRate);
        }
        else if (tc == 28)
        {
            // Aircraft status
            int st = data[4] & 0x7;   // Subtype
            if (st == 1)
            {
                int es = (data[5] >> 5) & 0x7; // Emergency state
                int modeA =  ((data[5] << 8) & 0x1f00) | (data[6] & 0xff); // Mode-A code (squawk)
                aircraft->m_status = emergencyStatus[es];
                aircraft->m_statusItem->setText(aircraft->m_status);
                int a, b, c, d;
                c = ((modeA >> 12) & 1) | ((modeA >> (10-1)) & 0x2) | ((modeA >> (8-2)) & 0x4);
                a = ((modeA >> 11) & 1) | ((modeA >> (9-1)) & 0x2) | ((modeA >> (7-2)) & 0x4);
                b = ((modeA >> 5) & 1) | ((modeA >> (3-1)) & 0x2) | ((modeA << (1)) & 0x4);
                d = ((modeA >> 4) & 1) | ((modeA >> (2-1)) & 0x2) | ((modeA << (2)) & 0x4);
                aircraft->m_squawk = a*1000 + b*100 + c*10 + d;
                if (modeA & 0x40)
                    aircraft->m_squawkItem->setText(QString("%1 IDENT").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                else
                    aircraft->m_squawkItem->setText(QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
            }
            else if (st == 2)
            {
                // TCAS/ACAS RA Broadcast
            }
        }
        else if (tc == 29)
        {
            // Target state and status
        }
        else if (tc == 31)
        {
            // Aircraft operation status
        }

        // Update aircraft in map
        if (aircraft->m_positionValid)
        {
            // Check to see if we need to start any animations
            QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);

            // Update map displayed in channel
            if (updateModel) {
                m_aircraftModel.aircraftUpdated(aircraft);
            }

            // Send to Map feature
            sendToMap(aircraft, animations);

            if (resetAnimation)
            {
                // Wait until after model has changed before reseting
                // otherwise animation might play on old model
                aircraft->m_gearDown = false;
                aircraft->m_flaps = 0.0;
                aircraft->m_engineStarted = false;
                aircraft->m_rotorStarted = false;
            }
        }
    }
    else if (df == 18)
    {
        // TIS-B
        qDebug() << "TIS B message cf=" << ca << " icao: " << icao;
    }

    // Check to see if we need to emit a notification about this aircraft
    checkDynamicNotification(aircraft);

    // Update text below photo if it's likely to have changed
    if ((aircraft == m_highlightAircraft) && (newAircraft || updatedCallsign)) {
        updatePhotoText(aircraft);
    }
}

QList<SWGSDRangel::SWGMapAnimation *> * ADSBDemodGUI::animate(QDateTime dateTime, Aircraft *aircraft)
{
    QList<SWGSDRangel::SWGMapAnimation *> *animations = new QList<SWGSDRangel::SWGMapAnimation *>();

    const int gearDownSpeed = 150;
    const int gearUpAltitude = 200;
    const int gearUpVerticalRate = 1000;
    const int accelerationHeight = 1500;
    const int flapsRetractAltitude = 2000;
    const int flapsCleanSpeed = 200;

    bool debug = false;

    // Landing gear should be down when on surface
    // Check speed in case we get a mixture of surface and airbourne positions
    // during take-off
    if (   aircraft->m_onSurface
        && !aircraft->m_gearDown
        && (   (aircraft->m_speedValid && (aircraft->m_speed < 80))
            || !aircraft->m_speedValid
           )
       )
    {
        if (debug) {
            qDebug() << "Gear down as on surface " << aircraft->m_icaoHex;
        }
        animations->append(gearAnimation(dateTime, false));
        aircraft->m_gearDown = true;
    }

    // Flaps when on the surface
    if (aircraft->m_onSurface && aircraft->m_speedValid)
    {
        if ((aircraft->m_speed <= 20) && (aircraft->m_flaps != 0.0))
        {
            // No flaps when stationary / taxiing
            if (debug) {
                qDebug() << "Parking flaps " << aircraft->m_icaoHex;
            }
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.0));
            animations->append(slatsAnimation(dateTime, true));
            aircraft->m_flaps = 0.0;
        }
        else if ((aircraft->m_speed >= 30) && (aircraft->m_flaps < 0.25))
        {
            // Flaps for takeoff
            if (debug) {
                qDebug() << "Takeoff flaps " << aircraft->m_icaoHex;
            }
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.25));
            animations->append(slatsAnimation(dateTime, false));
            aircraft->m_flaps = 0.25;
        }
    }

    // Pitch up on take-off
    if (   aircraft->m_gearDown
        && !aircraft->m_onSurface
        && (   (aircraft->m_verticalRateValid && (aircraft->m_verticalRate > 300))
            || (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + gearUpAltitude/2)))
           )
        )
    {
        if (debug) {
            qDebug() << "Pitch up " << aircraft->m_icaoHex;
        }
        aircraft->m_pitch = 5.0;
    }

    // Retract landing gear after take-off
    if (   aircraft->m_gearDown
        && !aircraft->m_onSurface
        && (   (aircraft->m_verticalRateValid && (aircraft->m_verticalRate > 1000))
            || (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + gearUpAltitude)))
           )
        )
    {
        if (debug) {
            qDebug() << "Gear up " << aircraft->m_icaoHex
                    << " VR " << (aircraft->m_verticalRateValid && (aircraft->m_verticalRate > gearUpVerticalRate))
                    << " Alt " << (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + gearUpAltitude)))
                    <<  "m_altitude " << aircraft->m_altitude   << " aircraft->m_runwayAltitude " << aircraft->m_runwayAltitude;
        }
        aircraft->m_pitch = 10.0;
        animations->append(gearAnimation(dateTime.addSecs(2), true));
        aircraft->m_gearDown = false;
    }

    // Reduce pitch at acceleration height
    if (   (aircraft->m_flaps > 0.0)
        && (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + accelerationHeight)))
       )
    {
        aircraft->m_pitch = 5.0;
    }

    // Retract flaps/slats after take-off
    // Should be after acceleration altitude (1500-3000ft AAL)
    // And before max speed for flaps is reached (215knt for A320, 255KIAS for 777)
    if (   (aircraft->m_flaps > 0.0)
        && (   (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + flapsRetractAltitude)))
            || (aircraft->m_speedValid && (aircraft->m_speed > flapsCleanSpeed))
           )
       )
    {
        if (debug) {
            qDebug() << "Retract flaps " << aircraft->m_icaoHex
                    << " Spd " << (aircraft->m_speedValid && (aircraft->m_speed > flapsCleanSpeed))
                    << " Alt " << (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + flapsRetractAltitude)));
        }
        animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.0));
        animations->append(slatsAnimation(dateTime, true));
        aircraft->m_flaps = 0.0;
        // Clear runway information
        aircraft->m_runwayAltitudeValid = false;
        aircraft->m_runwayAltitude = 0.0;
    }

    // Extend flaps for approach and landing
    // We don't know airport elevation, so just base on speed and descent rate
    // Vertical rate can go negative during take-off, so we check m_runwayAltitudeValid
    if (!aircraft->m_onSurface
        && !aircraft->m_runwayAltitudeValid
        && (aircraft->m_verticalRateValid && (aircraft->m_verticalRate < 0))
        && aircraft->m_speedValid
       )
    {
        if ((aircraft->m_speed < flapsCleanSpeed) && (aircraft->m_flaps < 0.25))
        {
            // Extend flaps for approach
            if (debug) {
                qDebug() << "Extend flaps for approach" << aircraft->m_icaoHex;
            }
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.25));
            //animations->append(slatsAnimation(dateTime, false));
            aircraft->m_flaps = 0.25;
            aircraft->m_pitch = 1.0;
        }
    }

    // Gear down for landing
    // We don't know airport elevation, so just base on speed and descent rate
    if (!aircraft->m_gearDown
        && !aircraft->m_onSurface
        && !aircraft->m_runwayAltitudeValid
        && (aircraft->m_verticalRateValid && (aircraft->m_verticalRate < 0))
        && (aircraft->m_speedValid && (aircraft->m_speed < gearDownSpeed))
       )
    {
        if (debug) {
            qDebug() << "Flaps/Gear down for landing " << aircraft->m_icaoHex;
        }
        animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.5));
        animations->append(slatsAnimation(dateTime, false));
        animations->append(gearAnimation(dateTime.addSecs(8), false));
        animations->append(flapsAnimation(dateTime.addSecs(16), 0.5, 1.0));
        aircraft->m_gearDown = true;
        aircraft->m_flaps = 1.0;
        aircraft->m_pitch = 3.0;
    }

    // Engine control
    if (aircraft->m_emitterCategory == "Rotorcraft")
    {
        // Helicopter rotors
        if (!aircraft->m_rotorStarted && !aircraft->m_onSurface)
        {
            // Start rotors
            if (debug) {
                qDebug() << "Start rotors " << aircraft->m_icaoHex;
            }
            animations->append(rotorAnimation(dateTime, false));
            aircraft->m_rotorStarted = true;
        }
        else if (aircraft->m_rotorStarted && aircraft->m_onSurface)
        {
            if (debug) {
                qDebug() << "Stop rotors " << aircraft->m_icaoHex;
            }
            animations->append(rotorAnimation(dateTime, true));
            aircraft->m_rotorStarted = false;
        }
    }
    else
    {
        // Propellors
        if (!aircraft->m_engineStarted && aircraft->m_speedValid && (aircraft->m_speed > 0))
        {
            if (debug) {
                qDebug() << "Start engines " << aircraft->m_icaoHex;
            }
            animations->append(engineAnimation(dateTime, 1, false));
            animations->append(engineAnimation(dateTime, 2, false));
            aircraft->m_engineStarted = true;
        }
        else if (aircraft->m_engineStarted && aircraft->m_speedValid && (aircraft->m_speed == 0))
        {
            if (debug) {
                qDebug() << "Stop engines " << aircraft->m_icaoHex;
            }
            animations->append(engineAnimation(dateTime, 1, true));
            animations->append(engineAnimation(dateTime, 2, true));
            aircraft->m_engineStarted = false;
        }
    }

    // Estimate pitch, so it looks a little more realistic
    if (aircraft->m_onSurface)
    {
        // Check speed so we don't set pitch to 0 immediately on touch-down
        // Should probably record time of touch-down and reduce over time
        if (aircraft->m_speedValid)
        {
            if (aircraft->m_speed < 80) {
                aircraft->m_pitch = 0.0;
            } else if ((aircraft->m_speed < 130) && (aircraft->m_pitch >= 2)) {
                aircraft->m_pitch = 1;
            }
        }
    }
    else if ((aircraft->m_flaps < 0.25) && aircraft->m_verticalRateValid)
    {
        // In climb/descent
        aircraft->m_pitch = std::abs(aircraft->m_verticalRate / 400.0);
    }

    // Estimate some roll
    if (aircraft->m_onSurface
        || (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude < (aircraft->m_runwayAltitude + accelerationHeight))))
    {
        aircraft->m_roll = 0.0;
    }
    else if (aircraft->m_headingValid)
    {
        // Really need to use more data points for this - or better yet, get it from Mode-S frames
        if (aircraft->m_prevHeadingDateTime.isValid())
        {
             qint64 msecs = aircraft->m_prevHeadingDateTime.msecsTo(aircraft->m_headingDateTime);
             if (msecs > 0)
             {
                 float headingDiff = fmod(aircraft->m_heading - aircraft->m_prevHeading + 540.0, 360.0) - 180.0;
                 float roll = headingDiff / (msecs / 1000.0);
                 //qDebug() << "Heading Diff " << headingDiff << " msecs " << msecs << " roll " << roll;
                 roll = std::min(roll, 15.0f);
                 roll = std::max(roll, -15.0f);
                 aircraft->m_roll = roll;
             }
        }
        aircraft->m_prevHeadingDateTime = aircraft->m_headingDateTime;
        aircraft->m_prevHeading = aircraft->m_heading;
    }

    return animations;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::gearAnimation(QDateTime startDateTime, bool up)
{
    // Gear up/down
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/gear_ratio"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(up);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::flapsAnimation(QDateTime startDateTime, float currentFlaps, float flaps)
{
    // Extend / retract flags
    bool retract = flaps < currentFlaps;
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/flap_ratio"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(retract);
    animation->setLoop(0);
    animation->setDuration(5*std::abs(flaps-currentFlaps));
    animation->setMultiplier(0.2);
    if (retract) {
        animation->setStartOffset(1.0 - currentFlaps);
    } else {
        animation->setStartOffset(currentFlaps);
    }
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::slatsAnimation(QDateTime startDateTime, bool retract)
{
    // Extend / retract slats
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/slat_ratio"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(retract);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::rotorAnimation(QDateTime startDateTime, bool stop)
{
    // Turn rotors in helicopter.glb model
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("Take 001"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(false);
    animation->setLoop(true);
    animation->setMultiplier(1);
    animation->setStop(stop);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::engineAnimation(QDateTime startDateTime, int engine, bool stop)
{
    // Turn propellors
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(QString("libxplanemp/engines/engine_rotation_angle_deg%1").arg(engine)));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(false);
    animation->setLoop(true);
    animation->setMultiplier(1);
    animation->setStop(stop);
    return animation;
}

void ADSBDemodGUI::checkStaticNotification(Aircraft *aircraft)
{
    for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
    {
        QString match;
        switch (m_settings.m_notificationSettings[i]->m_matchColumn)
        {
        case ADSB_COL_ICAO:
            match = aircraft->m_icaoItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_MODEL:
            match = aircraft->m_modelItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_REGISTRATION:
            match = aircraft->m_registrationItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_MANUFACTURER:
            match = aircraft->m_manufacturerNameItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_OWNER:
            match = aircraft->m_ownerItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_OPERATOR_ICAO:
            match = aircraft->m_operatorICAOItem->data(Qt::DisplayRole).toString();
            break;
        default:
            break;
        }
        if (!match.isEmpty())
        {
            if (m_settings.m_notificationSettings[i]->m_regularExpression.isValid())
            {
                if (m_settings.m_notificationSettings[i]->m_regularExpression.match(match).hasMatch())
                {
                    highlightAircraft(aircraft);

                    if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty()) {
                        speechNotification(aircraft, m_settings.m_notificationSettings[i]->m_speech);
                    }
                    if (!m_settings.m_notificationSettings[i]->m_command.isEmpty()) {
                        commandNotification(aircraft, m_settings.m_notificationSettings[i]->m_command);
                    }
                    if (m_settings.m_notificationSettings[i]->m_autoTarget) {
                        targetAircraft(aircraft);
                    }

                    aircraft->m_notified = true;
                }
            }
        }
    }
}

void ADSBDemodGUI::checkDynamicNotification(Aircraft *aircraft)
{
    if (!aircraft->m_notified)
    {
        for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
        {
            if (   (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_CALLSIGN)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_ALTITUDE)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_SPEED)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_RANGE)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_CATEGORY)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_STATUS)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_SQUAWK)
               )
            {
                QString match;
                switch (m_settings.m_notificationSettings[i]->m_matchColumn)
                {
                case ADSB_COL_CALLSIGN:
                    match = aircraft->m_callsignItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_ALTITUDE:
                    match = aircraft->m_altitudeItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_SPEED:
                    match = aircraft->m_speedItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_RANGE:
                    match = aircraft->m_rangeItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_CATEGORY:
                    match = aircraft->m_emitterCategoryItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_STATUS:
                    match = aircraft->m_statusItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_SQUAWK:
                    match = aircraft->m_squawkItem->data(Qt::DisplayRole).toString();
                    break;
                default:
                    break;
                }
                if (!match.isEmpty())
                {
                    if (m_settings.m_notificationSettings[i]->m_regularExpression.isValid())
                    {
                        if (m_settings.m_notificationSettings[i]->m_regularExpression.match(match).hasMatch())
                        {
                            highlightAircraft(aircraft);

                            if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty()) {
                                speechNotification(aircraft, m_settings.m_notificationSettings[i]->m_speech);
                            }
                            if (!m_settings.m_notificationSettings[i]->m_command.isEmpty()) {
                                commandNotification(aircraft, m_settings.m_notificationSettings[i]->m_command);
                            }
                            if (m_settings.m_notificationSettings[i]->m_autoTarget) {
                                targetAircraft(aircraft);
                            }

                            aircraft->m_notified = true;
                        }
                    }
                }
            }
        }
    }
}

void ADSBDemodGUI::speechNotification(Aircraft *aircraft, const QString &speech)
{
    m_speech->say(subAircraftString(aircraft, speech));
}

void ADSBDemodGUI::commandNotification(Aircraft *aircraft, const QString &command)
{
    QString commandLine = subAircraftString(aircraft, command);
    QStringList allArgs = commandLine.split(" ");

    if (allArgs.size() > 0)
    {
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }
}

QString ADSBDemodGUI::subAircraftString(Aircraft *aircraft, const QString &string)
{
    QString s = string;
    s = s.replace("${icao}", aircraft->m_icaoItem->data(Qt::DisplayRole).toString());
    s = s.replace("${callsign}", aircraft->m_callsignItem->data(Qt::DisplayRole).toString());
    s = s.replace("${aircraft}", aircraft->m_modelItem->data(Qt::DisplayRole).toString());
    s = s.replace("${latitude}", aircraft->m_latitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${longitude}", aircraft->m_longitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${altitude}", aircraft->m_altitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${speed}", aircraft->m_speedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${heading}", aircraft->m_headingItem->data(Qt::DisplayRole).toString());
    s = s.replace("${range}", aircraft->m_rangeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${azel}", aircraft->m_azElItem->data(Qt::DisplayRole).toString());
    s = s.replace("${category}", aircraft->m_emitterCategoryItem->data(Qt::DisplayRole).toString());
    s = s.replace("${status}", aircraft->m_statusItem->data(Qt::DisplayRole).toString());
    s = s.replace("${squawk}", aircraft->m_squawkItem->data(Qt::DisplayRole).toString());
    s = s.replace("${registration}", aircraft->m_registrationItem->data(Qt::DisplayRole).toString());
    s = s.replace("${manufacturer}", aircraft->m_manufacturerNameItem->data(Qt::DisplayRole).toString());
    s = s.replace("${owner}", aircraft->m_ownerItem->data(Qt::DisplayRole).toString());
    s = s.replace("${operator}", aircraft->m_operatorICAOItem->data(Qt::DisplayRole).toString());
    s = s.replace("${rssi}", aircraft->m_rssiItem->data(Qt::DisplayRole).toString());
    s = s.replace("${flightstatus}", aircraft->m_flightStatusItem->data(Qt::DisplayRole).toString());
    s = s.replace("${departure}", aircraft->m_depItem->data(Qt::DisplayRole).toString());
    s = s.replace("${arrival}", aircraft->m_arrItem->data(Qt::DisplayRole).toString());
    s = s.replace("${std}", aircraft->m_stdItem->data(Qt::DisplayRole).toString());
    s = s.replace("${etd}", aircraft->m_etdItem->data(Qt::DisplayRole).toString());
    s = s.replace("${atd}", aircraft->m_atdItem->data(Qt::DisplayRole).toString());
    s = s.replace("${sta}", aircraft->m_staItem->data(Qt::DisplayRole).toString());
    s = s.replace("${eta}", aircraft->m_etaItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ata}", aircraft->m_ataItem->data(Qt::DisplayRole).toString());
    return s;
}

bool ADSBDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        int sr = notif.getSampleRate();
        bool srTooLow = sr < 2000000;
        ui->warning->setVisible(srTooLow);
        if (srTooLow) {
            ui->warning->setText(QString("Sample rate must be >= 2000000. Currently %1").arg(sr));
        } else {
            ui->warning->setText("");
        }
        getRollupContents()->arrangeRollups();
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = sr;
        ui->deltaFrequency->setValueRange(false, 7, -sr/2, sr/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(sr/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (ADSBDemodReport::MsgReportADSB::match(message))
    {
        ADSBDemodReport::MsgReportADSB& report = (ADSBDemodReport::MsgReportADSB&) message;
        handleADSB(
            report.getData(),
            report.getDateTime(),
            report.getPreambleCorrelation(),
            report.getCorrelationOnes(),
            true);
        return true;
    }
    else if (ADSBDemodReport::MsgReportDemodStats::match(message))
    {
        ADSBDemodReport::MsgReportDemodStats& report = (ADSBDemodReport::MsgReportDemodStats&) message;
        if (m_settings.m_displayDemodStats)
        {
            ADSBDemodStats stats = report.getDemodStats();
            ui->stats->setText(QString("ADS-B: %1 Mode-S: %2 Matches: %3 CRC: %4 Type: %5 Avg Corr: %6 Demod Time: %7 Feed Time: %8").arg(stats.m_adsbFrames).arg(stats.m_modesFrames).arg(stats.m_correlatorMatches).arg(stats.m_crcFails).arg(stats.m_typeFails).arg(CalcDb::dbPower(m_correlationAvg.instantAverage()), 1, 'f', 1).arg(stats.m_demodTime, 1, 'f', 3).arg(stats.m_feedTime, 1, 'f', 3));
        }
        return true;
    }
    else if (ADSBDemod::MsgConfigureADSBDemod::match(message))
    {
        qDebug("ADSBDemodGUI::handleMessage: ADSBDemod::MsgConfigureADSBDemod");
        const ADSBDemod::MsgConfigureADSBDemod& cfg = (ADSBDemod::MsgConfigureADSBDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }

    return false;
}

void ADSBDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void ADSBDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void ADSBDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ADSBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void ADSBDemodGUI::on_rfBW_valueChanged(int value)
{
    Real bw = (Real)value;
    ui->rfBWText->setText(QString("%1M").arg(bw / 1000000.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void ADSBDemodGUI::on_threshold_valueChanged(int value)
{
    Real thresholddB = ((Real)value)/10.0f;
    ui->thresholdText->setText(QString("%1").arg(thresholddB, 0, 'f', 1));
    m_settings.m_correlationThreshold = thresholddB;
    applySettings();
}

void ADSBDemodGUI::on_phaseSteps_valueChanged(int value)
{
    ui->phaseStepsText->setText(QString("%1").arg(value));
    m_settings.m_interpolatorPhaseSteps = value;
    applySettings();
}

void ADSBDemodGUI::on_tapsPerPhase_valueChanged(int value)
{
    Real tapsPerPhase = ((Real)value)/10.0f;
    ui->tapsPerPhaseText->setText(QString("%1").arg(tapsPerPhase, 0, 'f', 1));
    m_settings.m_interpolatorTapsPerPhase = tapsPerPhase;
    applySettings();
}

void ADSBDemodGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feedEnabled = checked;
    // Don't disable host/port - so they can be entered before connecting
    applySettings();
    applyImportSettings();
}

void ADSBDemodGUI::on_notifications_clicked()
{
    ADSBDemodNotificationDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings();
    }
}

void ADSBDemodGUI::on_flightInfo_clicked()
{
    if (m_flightInformation)
    {
        // Selection mode is single, so only a single row should be returned
        QModelIndexList indexList = ui->adsbData->selectionModel()->selectedRows();
        if (!indexList.isEmpty())
        {
            int row = indexList.at(0).row();
            int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
            if (m_aircraft.contains(icao))
            {
                Aircraft *aircraft = m_aircraft.value(icao);
                if (!aircraft->m_flight.isEmpty())
                {
                    // Download flight information
                    m_flightInformation->getFlightInformation(aircraft->m_flight);
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No flight number for selected aircraft";
                }
            }
            else
            {
                qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No aircraft with icao " << icao;
            }
        }
        else
        {
            qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No aircraft selected";
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No flight information service - have you set an API key?";
    }
}

// Find highlighed aircraft on Map Feature
void ADSBDemodGUI::on_findOnMapFeature_clicked()
{
    QModelIndexList indexList = ui->adsbData->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        QString icao = ui->adsbData->item(row, 0)->text();
        FeatureWebAPIUtils::mapFind(icao);
    }
}

// Find aircraft on channel map
void ADSBDemodGUI::findOnChannelMap(Aircraft *aircraft)
{
    if (aircraft->m_positionValid)
    {
        QQuickItem *item = ui->map->rootObject();
        QObject *object = item->findChild<QObject*>("map");
        if(object != NULL)
        {
            QGeoCoordinate geocoord = object->property("center").value<QGeoCoordinate>();
            geocoord.setLatitude(aircraft->m_latitude);
            geocoord.setLongitude(aircraft->m_longitude);
            object->setProperty("center", QVariant::fromValue(geocoord));
        }
    }
}

void ADSBDemodGUI::adsbData_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->adsbData->itemAt(pos);
    if (item)
    {
        int row = item->row();
        int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
        Aircraft *aircraft = nullptr;
        if (m_aircraft.contains(icao)) {
            aircraft = m_aircraft.value(icao);
        }
        QString icaoHex = QString("%1").arg(icao, 1, 16);

        QMenu* tableContextMenu = new QMenu(ui->adsbData);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->addSeparator();

        // View aircraft on various websites

        QAction* planeSpottersAction = new QAction("View aircraft on planespotters.net...", tableContextMenu);
        connect(planeSpottersAction, &QAction::triggered, this, [icao]()->void {
            QString icaoUpper = QString("%1").arg(icao, 1, 16).toUpper();
            QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
        });
        tableContextMenu->addAction(planeSpottersAction);

        QAction* adsbExchangeAction = new QAction("View aircraft on adsbexchange.net...", tableContextMenu);
        connect(adsbExchangeAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://globe.adsbexchange.com/?icao=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(adsbExchangeAction);

        QAction* viewOpenSkyAction = new QAction("View aircraft on opensky-network.org...", tableContextMenu);
        connect(viewOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://opensky-network.org/aircraft-profile?icao24=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(viewOpenSkyAction);

        if (!aircraft->m_callsign.isEmpty())
        {
            QAction* flightRadarAction = new QAction("View flight on flightradar24.net...", tableContextMenu);
            connect(flightRadarAction, &QAction::triggered, this, [aircraft]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            });
            tableContextMenu->addAction(flightRadarAction);
        }

        tableContextMenu->addSeparator();

        // Edit aircraft

        if (!aircraft->m_aircraftInfo)
        {
            QAction* addOpenSkyAction = new QAction("Add aircraft to opensky-network.org...", tableContextMenu);
            connect(addOpenSkyAction, &QAction::triggered, this, []()->void {
                QDesktopServices::openUrl(QUrl(QString("https://opensky-network.org/edit-aircraft-profile")));
            });
            tableContextMenu->addAction(addOpenSkyAction);
        }
        else
        {

            QAction* editOpenSkyAction = new QAction("Edit aircraft on opensky-network.org...", tableContextMenu);
            connect(editOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://opensky-network.org/edit-aircraft-profile?icao24=%1").arg(icaoHex)));
            });
            tableContextMenu->addAction(editOpenSkyAction);
        }

        // Find on Map
        if (aircraft->m_positionValid)
        {
            tableContextMenu->addSeparator();

            QAction* findChannelMapAction = new QAction("Find on ADS-B map", tableContextMenu);
            connect(findChannelMapAction, &QAction::triggered, this, [this, aircraft]()->void {
                findOnChannelMap(aircraft);
            });
            tableContextMenu->addAction(findChannelMapAction);

            QAction* findMapFeatureAction = new QAction("Find on feature map", tableContextMenu);
            connect(findMapFeatureAction, &QAction::triggered, this, [icaoHex]()->void {
                FeatureWebAPIUtils::mapFind(icaoHex);
            });
            tableContextMenu->addAction(findMapFeatureAction);
        }

        tableContextMenu->popup(ui->adsbData->viewport()->mapToGlobal(pos));
    }
}

void ADSBDemodGUI::on_adsbData_cellClicked(int row, int column)
{
    (void) column;
    // Get ICAO of aircraft in row clicked
    int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
    if (m_aircraft.contains(icao)) {
         highlightAircraft(m_aircraft.value(icao));
    }
}

void ADSBDemodGUI::on_adsbData_cellDoubleClicked(int row, int column)
{
    // Get ICAO of aircraft in row double clicked
    int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
    if (column == ADSB_COL_ICAO)
    {
        // Search for aircraft on planespotters.net
        QString icaoUpper = QString("%1").arg(icao, 1, 16).toUpper();
        QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
    }
    else if (m_aircraft.contains(icao))
    {
        Aircraft *aircraft = m_aircraft.value(icao);

        if (column == ADSB_COL_CALLSIGN)
        {
            if (!aircraft->m_callsign.isEmpty())
            {
                // Search for flight on flightradar24
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            }
        }
        else
        {
            if (column == ADSB_COL_AZEL)
            {
                targetAircraft(aircraft);
            }
            // Center map view on aircraft if it has a valid position
            if (aircraft->m_positionValid)
            {
                findOnChannelMap(aircraft);
            }
        }
    }
}

// Columns in table reordered
void ADSBDemodGUI::adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;
    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void ADSBDemodGUI::adsbData_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;
    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in ADSB table header - show column select menu
void ADSBDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->adsbData->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void ADSBDemodGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;
    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->adsbData->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *ADSBDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

void ADSBDemodGUI::on_spb_currentIndexChanged(int value)
{
    m_settings.m_samplesPerBit = (value + 1) * 2;
    applySettings();
}

void ADSBDemodGUI::on_correlateFullPreamble_clicked(bool checked)
{
    m_settings.m_correlateFullPreamble = checked;
    applySettings();
}

void ADSBDemodGUI::on_demodModeS_clicked(bool checked)
{
    m_settings.m_demodModeS = checked;
    applySettings();
}

void ADSBDemodGUI::on_getOSNDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        QString osnDBFilename = getOSNDBZipFilename();
        if (confirmDownload(osnDBFilename))
        {
            // Download Opensky network database to a file
            QUrl dbURL(QString(OSNDB_URL));
            m_progressDialog = new QProgressDialog(this);
            m_progressDialog->setCancelButton(nullptr);
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(OSNDB_URL));
            QNetworkReply *reply = m_dlm.download(dbURL, osnDBFilename);
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
        }
    }
}

void ADSBDemodGUI::on_getAirportDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        QString airportDBFile = getAirportDBFilename();
        if (confirmDownload(airportDBFile))
        {
            // Download Opensky network database to a file
            QUrl dbURL(QString(AIRPORTS_URL));
            m_progressDialog = new QProgressDialog(this);
            m_progressDialog->setCancelButton(nullptr);
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(AIRPORTS_URL));
            QNetworkReply *reply = m_dlm.download(dbURL, airportDBFile);
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
        }
    }
}

void ADSBDemodGUI::on_getAirspacesDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setMaximum(OpenAIP::m_countryCodes.size());
        m_progressDialog->setCancelButton(nullptr);
        m_openAIP.downloadAirspaces();
    }
}

void ADSBDemodGUI::on_flightPaths_clicked(bool checked)
{
    m_settings.m_flightPaths = checked;
    m_aircraftModel.setFlightPaths(checked);
}

void ADSBDemodGUI::on_allFlightPaths_clicked(bool checked)
{
    m_settings.m_allFlightPaths = checked;
    m_aircraftModel.setAllFlightPaths(checked);
}

QString ADSBDemodGUI::getDataDir()
{
    // Get directory to store app data in (aircraft & airport databases and user-definable icons)
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString ADSBDemodGUI::getAirportDBFilename()
{
    return getDataDir() + "/airportDatabase.csv";
}

QString ADSBDemodGUI::getAirportFrequenciesDBFilename()
{
    return getDataDir() + "/airportFrequenciesDatabase.csv";
}

QString ADSBDemodGUI::getOSNDBZipFilename()
{
    return getDataDir() + "/aircraftDatabase.zip";
}

QString ADSBDemodGUI::getOSNDBFilename()
{
    return getDataDir() + "/aircraftDatabase.csv";
}

QString ADSBDemodGUI::getFastDBFilename()
{
    return getDataDir() + "/aircraftDatabaseFast.csv";
}

qint64 ADSBDemodGUI::fileAgeInDays(QString filename)
{
    QFile file(filename);
    if (file.exists())
    {
        QDateTime modified = file.fileTime(QFileDevice::FileModificationTime);
        if (modified.isValid())
            return modified.daysTo(QDateTime::currentDateTime());
        else
            return -1;
    }
    return -1;
}

bool ADSBDemodGUI::confirmDownload(QString filename)
{
    qint64 age = fileAgeInDays(filename);
    if ((age == -1) || (age > 100))
        return true;
    else
    {
        QMessageBox::StandardButton reply;
        if (age == 0)
            reply = QMessageBox::question(this, "Confirm download", "This file was last downloaded today. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else if (age == 1)
            reply = QMessageBox::question(this, "Confirm download", "This file was last downloaded yesterday. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else
            reply = QMessageBox::question(this, "Confirm download", QString("This file was last downloaded %1 days ago. Are you sure you wish to redownload this file?").arg(age), QMessageBox::Yes|QMessageBox::No);
        return reply == QMessageBox::Yes;
    }
}

bool ADSBDemodGUI::readOSNDB(const QString& filename)
{
     m_aircraftInfo = AircraftInformation::readOSNDB(filename);
     return m_aircraftInfo != nullptr;
}

bool ADSBDemodGUI::readFastDB(const QString& filename)
{
     m_aircraftInfo = AircraftInformation::readFastDB(filename);
     return m_aircraftInfo != nullptr;
}

void ADSBDemodGUI::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void ADSBDemodGUI::downloadFinished(const QString& filename, bool success)
{
    bool closeDialog = true;
    if (success)
    {
        if (filename == getOSNDBZipFilename())
        {
            // Extract .csv file from .zip file
            QZipReader reader(filename);
            QByteArray database = reader.fileData("media/data/samples/metadata/aircraftDatabase.csv");
            if (database.size() > 0)
            {
                QFile file(getOSNDBFilename());
                if (file.open(QIODevice::WriteOnly))
                {
                    file.write(database);
                    file.close();
                }
                else
                {
                    qWarning() << "ADSBDemodGUI::downloadFinished - Failed to open " << file.fileName() << " for writing";
                }
            }
            else
            {
                qWarning() << "ADSBDemodGUI::downloadFinished - aircraftDatabase.csv not in expected dir. Extracting all.";
                if (!reader.extractAll(getDataDir())) {
                    qWarning() << "ADSBDemodGUI::downloadFinished - Failed to extract files from " << filename;
                }
            }
            readOSNDB(getOSNDBFilename());
            // Convert to condensed format for faster loading later
            m_progressDialog->setLabelText("Processing.");
            AircraftInformation::writeFastDB(getFastDBFilename(), m_aircraftInfo);
        }
        else if (filename == getAirportDBFilename())
        {
            m_airportInfo = AirportInformation::readAirportsDB(filename);
            // Now download airport frequencies
            QUrl dbURL(QString(AIRPORT_FREQUENCIES_URL));
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(AIRPORT_FREQUENCIES_URL));
            QNetworkReply *reply = m_dlm.download(dbURL, getAirportFrequenciesDBFilename());
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
            closeDialog = false;
        }
        else if (filename == getAirportFrequenciesDBFilename())
        {
            if (m_airportInfo != nullptr)
            {
                AirportInformation::readFrequenciesDB(filename, m_airportInfo);
                // Update airports on map
                updateAirports();
            }
        }
        else
        {
            qDebug() << "ADSBDemodGUI::downloadFinished: Unexpected filename: " << filename;
        }
    }
    if (closeDialog && m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    if (rollupContents->hasExpandableWidgets()) {
        setSizePolicy(sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    } else {
        setSizePolicy(sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
    }

    int h = rollupContents->height() + getAdditionalHeight();
    resize(width(), h);

    rollupContents->saveState(m_rollupState);
    applySettings();
}

void ADSBDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_adsbDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

void ADSBDemodGUI::updateDeviceSetList()
{
    MainWindow *mainWindow = MainWindow::getInstance();
    std::vector<DeviceUISet*>& deviceUISets = mainWindow->getDeviceUISets();
    std::vector<DeviceUISet*>::const_iterator it = deviceUISets.begin();

    ui->device->blockSignals(true);

    ui->device->clear();
    unsigned int deviceIndex = 0;

    for (; it != deviceUISets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;

        if (deviceSourceEngine) {
            ui->device->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        }
    }

    int newDeviceIndex;

    if (it != deviceUISets.begin())
    {
        if (m_settings.m_deviceIndex < 0) {
            ui->device->setCurrentIndex(0);
        } else if (m_settings.m_deviceIndex >= (int) deviceUISets.size()) {
            ui->device->setCurrentIndex(deviceUISets.size() - 1);
        } else {
            ui->device->setCurrentIndex(m_settings.m_deviceIndex);
        }

        newDeviceIndex = ui->device->currentData().toInt();
    }
    else
    {
        newDeviceIndex = -1;
    }

    if (newDeviceIndex != m_settings.m_deviceIndex)
    {
        qDebug("ADSBDemodGUI::updateDeviceSetLists: device index changed: %d", newDeviceIndex);
        m_settings.m_deviceIndex = newDeviceIndex;
    }

    ui->device->blockSignals(false);
}

void ADSBDemodGUI::on_device_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_deviceIndex = ui->device->currentData().toInt();
        applySettings();
    }
}

QString ADSBDemodGUI::get3DModel(const QString &aircraftType, const QString &operatorICAO) const
{
    QString aircraftTypeOperator = aircraftType + "_" + operatorICAO;
    if (m_3DModels.contains(aircraftTypeOperator)) {
        return m_3DModels.value(aircraftTypeOperator);
    }
    if (m_settings.m_verboseModelMatching) {
        qDebug() << "ADS-B: No livery for " << aircraftTypeOperator;
    }
    return "";
}

QString ADSBDemodGUI::get3DModel(const QString &aircraftType)
{
    if (m_3DModelsByType.contains(aircraftType))
    {
        // Choose a random livery
        QStringList models = m_3DModelsByType.value(aircraftType);
        int size = models.size();
        int idx = m_random.bounded(size);
        return models[idx];
    }
    if (m_settings.m_verboseModelMatching) {
        qDebug() << "ADS-B: No aircraft for " << aircraftType;
    }
    return "";
}

void ADSBDemodGUI::get3DModel(Aircraft *aircraft)
{
    QString model;
    if (aircraft->m_aircraftInfo && !aircraft->m_aircraftInfo->m_model.isEmpty())
    {
        QString aircraftType;
        for (auto mm : m_3DModelMatch)
        {
            if (mm->match(aircraft->m_aircraftInfo->m_model, aircraft->m_aircraftInfo->m_manufacturerName, aircraftType))
            {
                // Look for operator specific livery
                if (!aircraft->m_aircraftInfo->m_operatorICAO.isEmpty()) {
                    model = get3DModel(aircraftType, aircraft->m_aircraftInfo->m_operatorICAO);
                }
                if (model.isEmpty()) {
                    // Try for aircraft with out specific livery
                    model = get3DModel(aircraftType);
                }
                if (!model.isEmpty())
                {
                    aircraft->m_aircraft3DModel = model;
                    if (m_modelAltitudeOffset.contains(aircraftType))
                    {
                        aircraft->m_modelAltitudeOffset = m_modelAltitudeOffset.value(aircraftType);
                        aircraft->m_labelAltitudeOffset = m_labelAltitudeOffset.value(aircraftType);
                    }
                }
                break;
            }
        }
        if (m_settings.m_verboseModelMatching)
        {
            if (model.isEmpty()) {
                qDebug() << "ADS-B: No 3D model for " << aircraft->m_aircraftInfo->m_model << " " << aircraft->m_aircraftInfo->m_operatorICAO << " for " << aircraft->m_icaoHex;
            } else {
                qDebug() << "ADS-B: Matched " << aircraft->m_aircraftInfo->m_model << " " << aircraft->m_aircraftInfo->m_operatorICAO << " to " << model << " for " << aircraft->m_icaoHex;
            }
        }
    }
}

void ADSBDemodGUI::get3DModelBasedOnCategory(Aircraft *aircraft)
{
    QString aircraftType;

    if (!aircraft->m_emitterCategory.compare("Heavy"))
    {
        QStringList heavy = {"B744", "B77W", "B788", "A388"};
        aircraftType = heavy[m_random.bounded(heavy.size())];
    }
    else if (!aircraft->m_emitterCategory.compare("Large"))
    {
        QStringList large = {"A319", "A320", "A321", "B737", "B738", "B739"};
        aircraftType = large[m_random.bounded(large.size())];
    }
    else if (!aircraft->m_emitterCategory.compare("Small"))
    {
        aircraftType = "LJ45";
    }
    else if (!aircraft->m_emitterCategory.compare("Rotorcraft"))
    {
        aircraft->m_aircraftCat3DModel = "helicopter.glb";
        aircraft->m_modelAltitudeOffset = 4.0f;
        aircraft->m_labelAltitudeOffset = 4.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("High performance"))
    {
        aircraft->m_aircraftCat3DModel = "f15.glb";
        aircraft->m_modelAltitudeOffset = 1.0f;
        aircraft->m_labelAltitudeOffset = 6.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("Light"))
    {
        aircraftType = "C172";
    }
    else if (!aircraft->m_emitterCategory.compare("Ultralight"))
    {
        aircraft->m_aircraftCat3DModel = "ultralight.glb";
        aircraft->m_modelAltitudeOffset = 0.55f;
        aircraft->m_labelAltitudeOffset = 0.75f;
    }
    else if (!aircraft->m_emitterCategory.compare("Glider/sailplane"))
    {
        aircraft->m_aircraftCat3DModel = "glider.glb";
        aircraft->m_modelAltitudeOffset = 1.0f;
        aircraft->m_labelAltitudeOffset = 1.5f;
    }
    else if (!aircraft->m_emitterCategory.compare("Space vehicle"))
    {
        aircraft->m_aircraftCat3DModel = "atlas_v.glb";
        aircraft->m_labelAltitudeOffset = 16.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("UAV"))
    {
        aircraft->m_aircraftCat3DModel = "drone.glb";
        aircraft->m_labelAltitudeOffset = 1.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("Emergency vehicle"))
    {
        aircraft->m_aircraftCat3DModel = "fire_truck.glb";
        aircraft->m_modelAltitudeOffset = 0.3f;
        aircraft->m_labelAltitudeOffset = 2.5f;
    }
    else if (!aircraft->m_emitterCategory.compare("Service vehicle"))
    {
        aircraft->m_aircraftCat3DModel = "airport_truck.glb";
        aircraft->m_labelAltitudeOffset = 3.0f;
    }
    else
    {
        aircraftType = "A320";
    }

    if (!aircraftType.isEmpty())
    {
        aircraft->m_aircraftCat3DModel = "";
        if (aircraft->m_aircraftInfo) {
            aircraft->m_aircraftCat3DModel = get3DModel(aircraftType, aircraft->m_aircraftInfo->m_operatorICAO);
        }
        if (aircraft->m_aircraftCat3DModel.isEmpty()) {
            aircraft->m_aircraftCat3DModel = get3DModel(aircraftType, aircraft->m_callsign.left(3));
        }
        if (aircraft->m_aircraftCat3DModel.isEmpty()) {
            aircraft->m_aircraftCat3DModel = get3DModel(aircraftType);
        }
        if (m_modelAltitudeOffset.contains(aircraftType))
        {
            aircraft->m_modelAltitudeOffset = m_modelAltitudeOffset.value(aircraftType);
            aircraft->m_labelAltitudeOffset = m_labelAltitudeOffset.value(aircraftType);
        }
    }
}

void ADSBDemodGUI::update3DModels()
{
    // Look for all aircraft gltfs in 3d directory
    QString modelDir = getDataDir() + "/3d";
    QStringList subDirs = {"BB_Airbus_png", "BB_Boeing_png", "BB_Jets_png", "BB_Props_png", "BB_GA_png", "BB_Mil_png", "BB_Heli_png"};

    for (auto subDir : subDirs)
    {
        QString dirName = modelDir + "/" + subDir;
        QDir dir(dirName);
        QStringList aircrafts = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for (auto aircraft : aircrafts)
        {
            if (m_settings.m_verboseModelMatching) {
                qDebug() << "Aircraft " << aircraft;
            }
            QDir aircraftDir(dir.filePath(aircraft));
            QStringList gltfs = aircraftDir.entryList({"*.gltf"});
            QStringList allAircraft;
            for (auto gltf : gltfs)
            {
                QStringList filenameParts = gltf.split(".")[0].split("_");
                if (filenameParts.size() == 2)
                {
                    QString livery = filenameParts[1];
                    if (m_settings.m_verboseModelMatching) {
                        qDebug() << "Aircraft " << aircraft << "Livery " << livery;
                    }
                    // Only use relative path, as Map feature will add the prefix
                    QString filename = subDir + "/" + aircraft + "/" + gltf;
                    m_3DModels.insert(aircraft + "_" + livery, filename);
                    allAircraft.append(filename);
                }
            }
            if (gltfs.size() > 0) {
                m_3DModelsByType.insert(aircraft, allAircraft);
            }
        }
    }

    // Vertical offset so undercarriage isn't underground, because 0,0,0 is in the middle of the model
    // rather than at the bottom
    m_modelAltitudeOffset.insert("A306", 4.6f);
    m_modelAltitudeOffset.insert("A310", 4.6f);
    m_modelAltitudeOffset.insert("A318", 3.7f);
    m_modelAltitudeOffset.insert("A319", 3.5f);
    m_modelAltitudeOffset.insert("A320", 3.5f);
    m_modelAltitudeOffset.insert("A321", 3.5f);
    m_modelAltitudeOffset.insert("A332", 5.52f);
    m_modelAltitudeOffset.insert("A333", 5.52f);
    m_modelAltitudeOffset.insert("A334", 5.52f);
    m_modelAltitudeOffset.insert("A343", 4.65f);
    m_modelAltitudeOffset.insert("A345", 4.65f);
    m_modelAltitudeOffset.insert("A346", 4.65f);
    m_modelAltitudeOffset.insert("A388", 5.75f);
    m_modelAltitudeOffset.insert("B717", 0.0f);
    m_modelAltitudeOffset.insert("B733", 3.1f);
    m_modelAltitudeOffset.insert("B734", 3.27f);
    m_modelAltitudeOffset.insert("B737", 3.0f);
    m_modelAltitudeOffset.insert("B738", 3.31f);
    m_modelAltitudeOffset.insert("B739", 3.32f);
    m_modelAltitudeOffset.insert("B74F", 5.3f);
    m_modelAltitudeOffset.insert("B744", 5.25f);
    m_modelAltitudeOffset.insert("B752", 3.6f);
    m_modelAltitudeOffset.insert("B763", 4.44f);
    m_modelAltitudeOffset.insert("B772", 5.57f);
    m_modelAltitudeOffset.insert("B773", 5.6f);
    m_modelAltitudeOffset.insert("B77L", 5.57f);
    m_modelAltitudeOffset.insert("B77W", 5.57f);
    m_modelAltitudeOffset.insert("B788", 4.1f);
    m_modelAltitudeOffset.insert("BE20", 1.48f);
    m_modelAltitudeOffset.insert("C150", 1.05f);
    m_modelAltitudeOffset.insert("C172", 1.16f);
    m_modelAltitudeOffset.insert("C421", 1.16f);
    m_modelAltitudeOffset.insert("H25B", 1.45f);
    m_modelAltitudeOffset.insert("LJ45", 1.27f);
    m_modelAltitudeOffset.insert("B462", 1.8f);
    m_modelAltitudeOffset.insert("B463", 1.9f);
    m_modelAltitudeOffset.insert("CRJ2", 1.3f);
    m_modelAltitudeOffset.insert("CRJ7", 1.66f);
    m_modelAltitudeOffset.insert("CRJ9", 2.27f);
    m_modelAltitudeOffset.insert("CRJX", 2.49f);
    m_modelAltitudeOffset.insert("DC10", 5.2f);
    m_modelAltitudeOffset.insert("E135", 1.88f);
    m_modelAltitudeOffset.insert("E145", 1.86f);
    m_modelAltitudeOffset.insert("E170", 2.3f);
    m_modelAltitudeOffset.insert("E190", 3.05f);
    m_modelAltitudeOffset.insert("E195", 2.97f);
    m_modelAltitudeOffset.insert("F28", 2.34f);
    m_modelAltitudeOffset.insert("F70", 2.43f);
    m_modelAltitudeOffset.insert("F100", 2.23f);
    m_modelAltitudeOffset.insert("J328", 1.01f);
    m_modelAltitudeOffset.insert("MD11", 5.22f);
    m_modelAltitudeOffset.insert("MD83", 2.71f);
    m_modelAltitudeOffset.insert("MD90", 2.62f);
    m_modelAltitudeOffset.insert("AT42", 1.75f);
    m_modelAltitudeOffset.insert("AT72", 1.83f);
    m_modelAltitudeOffset.insert("D328", 0.99f);
    m_modelAltitudeOffset.insert("DH8D", 1.65f);
    m_modelAltitudeOffset.insert("F50", 2.16f);
    m_modelAltitudeOffset.insert("JS41", 1.9f);
    m_modelAltitudeOffset.insert("L410", 1.1f);
    m_modelAltitudeOffset.insert("SB20", 2.0f);
    m_modelAltitudeOffset.insert("SF34", 1.89f);

    // Label offsets (from bottom of aircraft)
    m_labelAltitudeOffset.insert("A306", 10.0f);
    m_labelAltitudeOffset.insert("A310", 15.0f);
    m_labelAltitudeOffset.insert("A318", 10.0f);
    m_labelAltitudeOffset.insert("A319", 10.0f);
    m_labelAltitudeOffset.insert("A320", 10.0f);
    m_labelAltitudeOffset.insert("A321", 10.0f);
    m_labelAltitudeOffset.insert("A332", 14.0f);
    m_labelAltitudeOffset.insert("A333", 14.0f);
    m_labelAltitudeOffset.insert("A334", 14.0f);
    m_labelAltitudeOffset.insert("A343", 14.0f);
    m_labelAltitudeOffset.insert("A345", 14.0f);
    m_labelAltitudeOffset.insert("A346", 14.0f);
    m_labelAltitudeOffset.insert("A388", 20.0f);
    m_labelAltitudeOffset.insert("B717", 7.5f);
    m_labelAltitudeOffset.insert("B733", 10.0f);
    m_labelAltitudeOffset.insert("B734", 10.0f);
    m_labelAltitudeOffset.insert("B737", 10.0f);
    m_labelAltitudeOffset.insert("B738", 10.0f);
    m_labelAltitudeOffset.insert("B739", 10.0f);
    m_labelAltitudeOffset.insert("B74F", 15.0f);
    m_labelAltitudeOffset.insert("B744", 15.0f);
    m_labelAltitudeOffset.insert("B752", 12.0f);
    m_labelAltitudeOffset.insert("B763", 14.0f);
    m_labelAltitudeOffset.insert("B772", 14.0f);
    m_labelAltitudeOffset.insert("B773", 14.0f);
    m_labelAltitudeOffset.insert("B77L", 14.0f);
    m_labelAltitudeOffset.insert("B77W", 14.0f);
    m_labelAltitudeOffset.insert("B788", 14.0f);
    m_labelAltitudeOffset.insert("BE20", 4.0f);
    m_labelAltitudeOffset.insert("C150", 3.0f);
    m_labelAltitudeOffset.insert("C172", 3.0f);
    m_labelAltitudeOffset.insert("C421", 4.0f);
    m_labelAltitudeOffset.insert("H25B", 5.0f);
    m_labelAltitudeOffset.insert("LJ45", 5.0f);
    m_labelAltitudeOffset.insert("B462", 7.0f);
    m_labelAltitudeOffset.insert("B463", 7.0f);
    m_labelAltitudeOffset.insert("CRJ2", 5.5f);
    m_labelAltitudeOffset.insert("CRJ7", 6.0f);
    m_labelAltitudeOffset.insert("CRJ9", 6.0f);
    m_labelAltitudeOffset.insert("CRJX", 6.0f);
    m_labelAltitudeOffset.insert("DC10", 15.0f);
    m_labelAltitudeOffset.insert("E135", 5.0f);
    m_labelAltitudeOffset.insert("E145", 5.0f);
    m_labelAltitudeOffset.insert("E170", 8.0f);
    m_labelAltitudeOffset.insert("E190", 8.5f);
    m_labelAltitudeOffset.insert("E195", 8.5f);
    m_labelAltitudeOffset.insert("F28", 7.0f);
    m_labelAltitudeOffset.insert("F70", 6.5f);
    m_labelAltitudeOffset.insert("F100", 6.5f);
    m_labelAltitudeOffset.insert("J328", 5.0f);  // Check
    m_labelAltitudeOffset.insert("MD11", 15.0f);
    m_labelAltitudeOffset.insert("MD83", 7.5f);
    m_labelAltitudeOffset.insert("MD90", 7.5f);
    m_labelAltitudeOffset.insert("AT42", 7.0f);
    m_labelAltitudeOffset.insert("AT72", 7.0f);
    m_labelAltitudeOffset.insert("D328", 6.0f);
    m_labelAltitudeOffset.insert("DH8D", 6.5f);
    m_labelAltitudeOffset.insert("F50",  7.0f);
    m_labelAltitudeOffset.insert("JS41", 5.0f);
    m_labelAltitudeOffset.insert("L410", 5.0f);
    m_labelAltitudeOffset.insert("SB20", 6.5f);
    m_labelAltitudeOffset.insert("SF34", 6.0f);

    // Map from database names to 3D model names
    m_3DModelMatch.append(new ModelMatch("A300.*", "A306")); // A300 B4 is A300-600, but use for others as closest match
    m_3DModelMatch.append(new ModelMatch("A310.*", "A310"));
    m_3DModelMatch.append(new ModelMatch("A318.*", "A318"));
    m_3DModelMatch.append(new ModelMatch("A.?319.*", "A319"));
    m_3DModelMatch.append(new ModelMatch("A.?320.*", "A320"));
    m_3DModelMatch.append(new ModelMatch("A.?321.*", "A321"));
    m_3DModelMatch.append(new ModelMatch("A330.2.*", "A332"));
    m_3DModelMatch.append(new ModelMatch("A330.3.*", "A333"));
    m_3DModelMatch.append(new ModelMatch("A330.4.*", "A342"));
    m_3DModelMatch.append(new ModelMatch("A340.3.*", "A343"));
    m_3DModelMatch.append(new ModelMatch("A340.5.*", "A345"));
    m_3DModelMatch.append(new ModelMatch("A340.6.*", "A346"));
    m_3DModelMatch.append(new ModelMatch("A350.*", "A333"));   // No A350 model - use 330 as twin engine
    m_3DModelMatch.append(new ModelMatch("A380.*", "A388"));

    m_3DModelMatch.append(new ModelMatch("737.2.*", "B733"));  // No 200 model
    m_3DModelMatch.append(new ModelMatch("737.3.*", "B733"));
    m_3DModelMatch.append(new ModelMatch("737.4.*", "B734"));
    m_3DModelMatch.append(new ModelMatch("737.5.*", "B734"));  // No 500 model
    m_3DModelMatch.append(new ModelMatch("737.6.*", "B737"));  // No 600 model
    m_3DModelMatch.append(new ModelMatch("737NG.6.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737.7.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737NG.7.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737.8.*", "B738"));
    m_3DModelMatch.append(new ModelMatch("737NG.8.*", "B738"));  // No Max model yet
    m_3DModelMatch.append(new ModelMatch("737MAX.8.*", "B738"));
    m_3DModelMatch.append(new ModelMatch("737.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("737NG.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("737MAX.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("B747.*F", "B74F"));
    m_3DModelMatch.append(new ModelMatch("B747.*\\(F\\)", "B74F"));
    m_3DModelMatch.append(new ModelMatch("747.*", "B744"));
    m_3DModelMatch.append(new ModelMatch("757.*", "B752"));
    m_3DModelMatch.append(new ModelMatch("767.*", "B763"));
    m_3DModelMatch.append(new ModelMatch("777.2.*LR.*", "B77L"));
    m_3DModelMatch.append(new ModelMatch("777.2.*", "B772"));
    m_3DModelMatch.append(new ModelMatch("777.3.*ER.*", "B77W"));
    m_3DModelMatch.append(new ModelMatch("777.3.*", "B773"));
    m_3DModelMatch.append(new ModelMatch("777.*", "B772"));
    m_3DModelMatch.append(new ModelMatch("787.*", "B788"));
    m_3DModelMatch.append(new ModelMatch("717.*", "B717"));
    // No 727 model

    // Jets
    m_3DModelMatch.append(new ModelMatch(".*EMB.135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch("Embraer 135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch("Embraer 145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch("Embraer 170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch("Embraer 190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.195.*", "E195"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.195.*", "E195"));
    m_3DModelMatch.append(new ModelMatch("Embraer 195.*", "E195"));

    m_3DModelMatch.append(new ModelMatch(".*CRJ.200.*", "CRJ2"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.700.*", "CRJ7"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.900.*", "CRJ9"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.1000.*", "CRJX"));

    // PNGs missing
    //m_3DModelMatch.append(new ModelMatch("(BAE )?146.2.*", "B462"));
    //m_3DModelMatch.append(new ModelMatch("(BAE )?146.3.*", "B463"));

    m_3DModelMatch.append(new ModelMatch("DC-10.*", "DC10"));

    m_3DModelMatch.append(new ModelMatch(".*MD.11.*", "MD11"));
    m_3DModelMatch.append(new ModelMatch(".*MD.83.*", "MD83"));
    m_3DModelMatch.append(new ModelMatch(".*MD.90.*", "MD90"));

    m_3DModelMatch.append(new ModelMatch(".*F28.*", "F28"));
    m_3DModelMatch.append(new ModelMatch(".*F70.*", "F70"));
    m_3DModelMatch.append(new ModelMatch(".*F100.*", "F100"));

    // GA
    m_3DModelMatch.append(new ModelMatch(".*B200.*", "BE20"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*200.*", ".*Beech.*", "BE20"));
    m_3DModelMatch.append(new ModelMatch(".*150.*", "C150"));
    m_3DModelMatch.append(new ModelMatch(".*172.*", "C172"));
    m_3DModelMatch.append(new ModelMatch(".*421.*", "C421"));
    m_3DModelMatch.append(new ModelMatch(".*125.*", "H25B"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*400.*", "Hawker.*", "H25B"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*400.*", "Raytheon.*", "H25B"));
    m_3DModelMatch.append(new ModelMatch(".*Learjet.*", "LJ45"));

    // Props
    m_3DModelMatch.append(new ModelMatch("ATR.*42.*", "AT42"));
    m_3DModelMatch.append(new ModelMatch("ATR.*72.*", "AT72"));
    m_3DModelMatch.append(new ModelMatch("Do 328.*", "D328"));
    m_3DModelMatch.append(new ModelMatch("DHC-8.*", "DH8D"));
    m_3DModelMatch.append(new ModelMatch(".*F50.*", "F50"));
    m_3DModelMatch.append(new ModelMatch("Jetstream 41.*", "JS41"));
    m_3DModelMatch.append(new ModelMatch(".*L.410.*", "L410"));
    m_3DModelMatch.append(new ModelMatch("SAAB.2000.*", "SB20"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*340.*", "Saab.*", "SF34"));
}

void ADSBDemodGUI::updateAirports()
{
    m_airportModel.removeAllAirports();
    QHash<int, AirportInformation *>::iterator i = m_airportInfo->begin();
    AzEl azEl = m_azEl;

    while (i != m_airportInfo->end())
    {
        AirportInformation *airportInfo = i.value();

        // Calculate distance and az/el to airport from My Position
        azEl.setTarget(airportInfo->m_latitude, airportInfo->m_longitude, Units::feetToMetres(airportInfo->m_elevation));
        azEl.calculate();

        // Only display airport if in range
        if (azEl.getDistance() <= m_settings.m_airportRange*1000.0f)
        {
            // Only display the airport if it's large enough
            if (airportInfo->m_type >= m_settings.m_airportMinimumSize)
            {
                // Only display heliports if enabled
                if (m_settings.m_displayHeliports || (airportInfo->m_type != ADSBDemodSettings::AirportType::Heliport))
                {
                    m_airportModel.addAirport(airportInfo, azEl.getAzimuth(), azEl.getElevation(), azEl.getDistance());
                }
            }
        }
        ++i;
    }
    // Save settings we've just used so we know if they've changed
    m_currentAirportMinimumSize = m_settings.m_airportMinimumSize;
    m_currentDisplayHeliports = m_settings.m_displayHeliports;
}

void ADSBDemodGUI::updateAirspaces()
{
    AzEl azEl = m_azEl;
    m_airspaceModel.removeAllAirspaces();
    for (const auto& airspace: m_airspaces)
    {
        if (m_settings.m_airspaces.contains(airspace->m_category))
        {
            // Calculate distance to airspace from My Position
            azEl.setTarget(airspace->m_center.y(), airspace->m_center.x(), 0);
            azEl.calculate();

            // Only display airport if in range
            if (azEl.getDistance() <= m_settings.m_airspaceRange*1000.0f) {
                m_airspaceModel.addAirspace(airspace);
            }
        }
    }
}

void ADSBDemodGUI::updateNavAids()
{
    AzEl azEl = m_azEl;
    m_navAidModel.removeAllNavAids();
    if (m_settings.m_displayNavAids)
    {
        for (const auto& navAid: m_navAids)
        {
            // Calculate distance to NavAid from My Position
            azEl.setTarget(navAid->m_latitude, navAid->m_longitude, Units::feetToMetres(navAid->m_elevation));
            azEl.calculate();

            // Only display NavAid if in range
            if (azEl.getDistance() <= m_settings.m_airspaceRange*1000.0f) {
                m_navAidModel.addNavAid(navAid);
            }
        }
    }
}

// Set a static target, such as an airport
void ADSBDemodGUI::target(const QString& name, float az, float el, float range)
{
    if (m_trackAircraft)
    {
        // Restore colour of current target
        m_trackAircraft->m_isTarget = false;
        m_aircraftModel.aircraftUpdated(m_trackAircraft);
        m_trackAircraft = nullptr;
    }
    m_adsbDemod->setTarget(name, az, el, range);
}

void ADSBDemodGUI::targetAircraft(Aircraft *aircraft)
{
    if (aircraft != m_trackAircraft)
    {
        if (m_trackAircraft)
        {
            // Restore colour of current target
            m_trackAircraft->m_isTarget = false;
            m_aircraftModel.aircraftUpdated(m_trackAircraft);
        }
        // Track this aircraft
        m_trackAircraft = aircraft;
        if (aircraft->m_positionValid)
            m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
        // Change colour of new target
        aircraft->m_isTarget = true;
        m_aircraftModel.aircraftUpdated(aircraft);
    }
}

void ADSBDemodGUI::updatePhotoText(Aircraft *aircraft)
{
    if (m_settings.m_displayPhotos)
    {
        QString callsign = aircraft->m_callsignItem->text().trimmed();
        QString reg = aircraft->m_registrationItem->text().trimmed();
        if (!callsign.isEmpty() && !reg.isEmpty()) {
            ui->photoHeader->setText(QString("%1 - %2").arg(callsign).arg(reg));
        } else if (!callsign.isEmpty()) {
            ui->photoHeader->setText(QString("%1").arg(callsign));
        } else if (!reg.isEmpty()) {
            ui->photoHeader->setText(QString("%1").arg(reg));
        }

        QIcon icon = aircraft->m_countryItem->icon();
        QList<QSize> sizes = icon.availableSizes();
        if (sizes.size() > 0) {
            ui->photoFlag->setPixmap(icon.pixmap(sizes[0]));
        } else {
            ui->photoFlag->setPixmap(QPixmap());
        }

        updatePhotoFlightInformation(aircraft);

        QString aircraftDetails = "<table width=200>"; // Note, Qt seems to make the table bigger than this so text is cropped, not wrapped
        QString manufacturer = aircraft->m_manufacturerNameItem->text();
        if (!manufacturer.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Manufacturer:<td>%1").arg(manufacturer));
        }
        QString model = aircraft->m_modelItem->text();
        if (!model.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Aircraft:<td>%1").arg(model));
        }
        QString owner = aircraft->m_ownerItem->text();
        if (!owner.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Owner:<td>%1").arg(owner));
        }
        QString operatorICAO = aircraft->m_operatorICAOItem->text();
        if (!operatorICAO.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Operator:<td>%1").arg(operatorICAO));
        }
        QString registered = aircraft->m_registeredItem->text();
        if (!registered.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Registered:<td>%1").arg(registered));
        }
        aircraftDetails.append("</table>");
        ui->aircraftDetails->setText(aircraftDetails);
    }
}

void ADSBDemodGUI::updatePhotoFlightInformation(Aircraft *aircraft)
{
    if (m_settings.m_displayPhotos)
    {
        QString dep = aircraft->m_depItem->text();
        QString arr = aircraft->m_arrItem->text();
        QString std = aircraft->m_stdItem->text();
        QString etd = aircraft->m_etdItem->text();
        QString atd = aircraft->m_atdItem->text();
        QString sta = aircraft->m_staItem->text();
        QString eta = aircraft->m_etaItem->text();
        QString ata = aircraft->m_ataItem->text();
        QString flightDetails;
        if (!dep.isEmpty() && !arr.isEmpty())
        {
            flightDetails = QString("<center><table width=200><tr><th colspan=4>%1 - %2").arg(dep).arg(arr);
            if (!std.isEmpty() && !sta.isEmpty()) {
                flightDetails.append(QString("<tr><td>STD<td>%1<td>STA<td>%2").arg(std).arg(sta));
            }
            if ((!atd.isEmpty() || !etd.isEmpty()) && (!ata.isEmpty() || !eta.isEmpty()))
            {
                if (!atd.isEmpty()) {
                    flightDetails.append(QString("<tr><td>Actual<td>%1").arg(atd));
                } else if (!etd.isEmpty()) {
                    flightDetails.append(QString("<tr><td>Estimated<td>%1").arg(etd));
                }
                if (!ata.isEmpty()) {
                    flightDetails.append(QString("<td>Actual<td>%1").arg(ata));
                } else if (!eta.isEmpty()) {
                    flightDetails.append(QString("<td>Estimated<td>%1").arg(eta));
                }
            }
            flightDetails.append("</center>");
        }
        ui->flightDetails->setText(flightDetails);
    }
}

void ADSBDemodGUI::highlightAircraft(Aircraft *aircraft)
{
    if (aircraft != m_highlightAircraft)
    {
        // Hide photo of old aircraft
        ui->photoHeader->setVisible(false);
        ui->photoFlag->setVisible(false);
        ui->photo->setVisible(false);
        ui->flightDetails->setVisible(false);
        ui->aircraftDetails->setVisible(false);
        if (m_highlightAircraft)
        {
            // Restore colour
            m_highlightAircraft->m_isHighlighted = false;
            m_aircraftModel.aircraftUpdated(m_highlightAircraft);
        }
        // Highlight this aircraft
        m_highlightAircraft = aircraft;
        if (aircraft)
        {
            aircraft->m_isHighlighted = true;
            m_aircraftModel.aircraftUpdated(aircraft);
            if (m_settings.m_displayPhotos)
            {
                // Download photo
                updatePhotoText(aircraft);
                m_planeSpotters.getAircraftPhoto(aircraft->m_icaoHex);
            }
        }
    }
    if (aircraft)
    {
        // Highlight the row in the table - always do this, as it can become
        // unselected
        ui->adsbData->selectRow(aircraft->m_icaoItem->row());
    }
    else
    {
        ui->adsbData->clearSelection();
    }
}

// Show feed dialog
void ADSBDemodGUI::feedSelect()
{
    ADSBDemodFeedDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
        applyImportSettings();
    }
}

// Show display settings dialog
void ADSBDemodGUI::on_displaySettings_clicked()
{
    bool oldSiUnits = m_settings.m_siUnits;
    ADSBDemodDisplayDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        bool unitsChanged = m_settings.m_siUnits != oldSiUnits;
        if (unitsChanged) {
            m_aircraftModel.allAircraftUpdated();
        }
        displaySettings();
        applySettings();
    }
}

void ADSBDemodGUI::applyMapSettings()
{
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();

    QQuickItem *item = ui->map->rootObject();

    QObject *object = item->findChild<QObject*>("map");
    QGeoCoordinate coords;
    double zoom;
    if (object != nullptr)
    {
        // Save existing position of map
        coords = object->property("center").value<QGeoCoordinate>();
        zoom = object->property("zoomLevel").value<double>();
    }
    else
    {
        // Center on my location when map is first opened
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        zoom = 10.0;
    }

    // Create the map using the specified provider
    QQmlProperty::write(item, "mapProvider", m_settings.m_mapProvider);
    QVariantMap parameters;
    QString mapType;

    if (m_settings.m_mapProvider == "osm")
    {
        // Use our repo, so we can append API key and redefine transmit maps
        parameters["osm.mapping.providersrepository.address"] = QString("http://127.0.0.1:%1/").arg(m_osmPort);
        // Use ADS-B specific cache, as we use different transmit maps
        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/QtLocation/5.8/tiles/osm/sdrangel_adsb";
        parameters["osm.mapping.cache.directory"] = cachePath;
        // On Linux, we need to create the directory
        QDir dir(cachePath);
        if (!dir.exists()) {
            dir.mkpath(cachePath);
        }
        switch (m_settings.m_mapType)
        {
        case ADSBDemodSettings::AVIATION_LIGHT:
            mapType = "Transit Map";
            break;
        case ADSBDemodSettings::AVIATION_DARK:
            mapType = "Night Transit Map";
            break;
        case ADSBDemodSettings::STREET:
            mapType = "Street Map";
            break;
        case ADSBDemodSettings::SATELLITE:
            mapType = "Satellite Map";
            break;
        }
    }
    else if (m_settings.m_mapProvider == "mapboxgl")
    {
        switch (m_settings.m_mapType)
        {
        case ADSBDemodSettings::AVIATION_LIGHT:
            mapType = "mapbox://styles/mapbox/light-v9";
            break;
        case ADSBDemodSettings::AVIATION_DARK:
            mapType = "mapbox://styles/mapbox/dark-v9";
            break;
        case ADSBDemodSettings::STREET:
            mapType = "mapbox://styles/mapbox/streets-v10";
            break;
        case ADSBDemodSettings::SATELLITE:
            mapType = "mapbox://styles/mapbox/satellite-v9";
            break;
        }
    }

    QVariant retVal;
    if (!QMetaObject::invokeMethod(item, "createMap", Qt::DirectConnection,
                                Q_RETURN_ARG(QVariant, retVal),
                                Q_ARG(QVariant, QVariant::fromValue(parameters)),
                                Q_ARG(QVariant, mapType),
                                Q_ARG(QVariant, QVariant::fromValue(this))))
    {
        qCritical() << "ADSBDemodGUI::applyMapSettings - Failed to invoke createMap";
    }
    QObject *newMap = retVal.value<QObject *>();

    // Restore position of map
    if (newMap != nullptr)
    {
        if (coords.isValid())
        {
            newMap->setProperty("zoomLevel", QVariant::fromValue(zoom));
            newMap->setProperty("center", QVariant::fromValue(coords));
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI::applyMapSettings - createMap returned a nullptr";
    }

    // Move antenna icon to My Position
    QObject *stationObject = newMap->findChild<QObject*>("station");
    if(stationObject != NULL)
    {
        QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        stationObject->setProperty("coordinate", QVariant::fromValue(coords));
        stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
    }
    else
    {
        qDebug() << "ADSBDemodGUI::applyMapSettings - Couldn't find station";
    }
}

// Called from QML when empty space clicked
void ADSBDemodGUI::clearHighlighted()
{
    highlightAircraft(nullptr);
}

ADSBDemodGUI::ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ADSBDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_basicSettingsShown(false),
    m_doApplySettings(true),
    m_tickCount(0),
    m_aircraftInfo(nullptr),
    m_airportModel(this),
    m_airspaceModel(this),
    m_trackAircraft(nullptr),
    m_highlightAircraft(nullptr),
    m_progressDialog(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodadsb/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_osmPort = 0; // Pick a free port
    m_templateServer = new ADSBOSMTemplateServer("q2RVNAe3eFKCH4XsrE3r", m_osmPort);

    ui->map->rootContext()->setContextProperty("aircraftModel", &m_aircraftModel);
    ui->map->rootContext()->setContextProperty("airportModel", &m_airportModel);
    ui->map->rootContext()->setContextProperty("airspaceModel", &m_airspaceModel);
    ui->map->rootContext()->setContextProperty("navAidModel", &m_navAidModel);
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map.qml")));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &ADSBDemodGUI::downloadFinished);

    m_adsbDemod = reinterpret_cast<ADSBDemod*>(rxChannel); //new ADSBDemod(m_deviceUISet->m_deviceSourceAPI);
    m_adsbDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(feedSelect()));

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    ui->warning->setVisible(false);
    ui->warning->setStyleSheet("QLabel { background-color: red; }");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("ADS-B Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Set size of airline icons
    ui->adsbData->setIconSize(QSize(85, 20));
    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->adsbData->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->adsbData->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->adsbData);
    for (int i = 0; i < ui->adsbData->horizontalHeader()->count(); i++)
    {
        QString text = ui->adsbData->horizontalHeaderItem(i)->text();
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->adsbData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->adsbData->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->adsbData->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(adsbData_sectionMoved(int, int, int)));
    connect(ui->adsbData->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(adsbData_sectionResized(int, int, int)));
    // Context menu
    ui->adsbData->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->adsbData, SIGNAL(customContextMenuRequested(QPoint)), SLOT(adsbData_customContextMenuRequested(QPoint)));

    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->flightDetails->setVisible(false);
    ui->aircraftDetails->setVisible(false);

    // Read aircraft information database, if it has previously been downloaded
    if (!readFastDB(getFastDBFilename()))
    {
        if (readOSNDB(getOSNDBFilename()))
            AircraftInformation::writeFastDB(getFastDBFilename(), m_aircraftInfo);
    }
    // Read airport information database, if it has previously been downloaded
    m_airportInfo = AirportInformation::readAirportsDB(getAirportDBFilename());
    if (m_airportInfo != nullptr)
        AirportInformation::readFrequenciesDB(getAirportFrequenciesDBFilename(), m_airportInfo);
    // Read registration prefix to country map
    m_prefixMap = CSV::hash(":/flags/regprefixmap.csv");
    // Read operator air force to military map
    m_militaryMap = CSV::hash(":/flags/militarymap.csv");

    connect(&m_openAIP, &OpenAIP::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_openAIP, &OpenAIP::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &ADSBDemodGUI::downloadAirspaceFinished);
    connect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &ADSBDemodGUI::downloadNavAidsFinished);

    // Read airspaces
    m_airspaces = OpenAIP::readAirspaces();
    // Read NavAids
    m_navAids = OpenAIP::readNavAids();

    // Get station position
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

    // These are the default values in sdrbase/settings/preferences.cpp
    if ((stationLatitude == 49.012423) && (stationLongitude == 8.418125)) {
        ui->warning->setText("Please set your antenna location under Preferences > My Position");
    }

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ADSBDemodGUI::preferenceChanged);

    // Get airport weather when requested
    connect(&m_airportModel, &AirportModel::requestMetar, this, &ADSBDemodGUI::requestMetar);

    // Add airports within range of My Position
    if (m_airportInfo != nullptr) {
        updateAirports();
    }
    updateAirspaces();
    updateNavAids();
    update3DModels();

    // Initialise text to speech engine
    m_speech = new QTextToSpeech(this);

    m_flightInformation = nullptr;
    m_aviationWeather = nullptr;

    connect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    connect(ui->photo, &ClickableLabel::clicked, this, &ADSBDemodGUI::photoClicked);

    // Update device list when devices are added or removed
    connect(MainCore::instance(), &MainCore::deviceSetAdded, this, &ADSBDemodGUI::updateDeviceSetList);
    connect(MainCore::instance(), &MainCore::deviceSetRemoved, this, &ADSBDemodGUI::updateDeviceSetList);

    updateDeviceSetList();
    displaySettings();
    makeUIConnections();
    applySettings(true);

    connect(&m_importTimer, &QTimer::timeout, this, &ADSBDemodGUI::import);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ADSBDemodGUI::handleImportReply
    );
    applyImportSettings();

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &ADSBDemodGUI::redrawMap);
    m_redrawMapTimer.setSingleShot(true);
    ui->map->installEventFilter(this);
}

ADSBDemodGUI::~ADSBDemodGUI()
{
    if (m_templateServer)
    {
        m_templateServer->close();
        delete m_templateServer;
    }
    disconnect(&m_openAIP, &OpenAIP::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    disconnect(&m_openAIP, &OpenAIP::downloadError, this, &ADSBDemodGUI::downloadError);
    disconnect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &ADSBDemodGUI::downloadAirspaceFinished);
    disconnect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &ADSBDemodGUI::downloadNavAidsFinished);
    disconnect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    disconnect(&m_redrawMapTimer, &QTimer::timeout, this, &ADSBDemodGUI::redrawMap);
    m_redrawMapTimer.stop();
    delete ui;
    qDeleteAll(m_aircraft);
    if (m_airportInfo) {
        qDeleteAll(*m_airportInfo);
    }
    if (m_aircraftInfo) {
        qDeleteAll(*m_aircraftInfo);
    }
    qDeleteAll(m_airlineIcons);
    qDeleteAll(m_flagIcons);
    if (m_flightInformation)
    {
        disconnect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodGUI::flightInformationUpdated);
        delete m_flightInformation;
    }
    if (m_aviationWeather)
    {
        delete m_aviationWeather;
    }
    qDeleteAll(m_airspaces);
    qDeleteAll(m_navAids);
    qDeleteAll(m_3DModelMatch);
    delete m_networkManager;
}

void ADSBDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        qDebug() << "ADSBDemodGUI::applySettings";

        ADSBDemod::MsgConfigureADSBDemod* message = ADSBDemod::MsgConfigureADSBDemod::create(m_settings, force);
        m_adsbDemod->getInputMessageQueue()->push(message);
    }
}

void ADSBDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1000000.0, 0, 'f', 1));
    ui->rfBW->setValue((int)m_settings.m_rfBandwidth);

    ui->spb->setCurrentIndex(m_settings.m_samplesPerBit/2-1);
    ui->correlateFullPreamble->setChecked(m_settings.m_correlateFullPreamble);
    ui->demodModeS->setChecked(m_settings.m_demodModeS);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 1));
    ui->threshold->setValue((int)(m_settings.m_correlationThreshold*10.0f));

    ui->phaseStepsText->setText(QString("%1").arg(m_settings.m_interpolatorPhaseSteps));
    ui->phaseSteps->setValue(m_settings.m_interpolatorPhaseSteps);
    ui->tapsPerPhaseText->setText(QString("%1").arg(m_settings.m_interpolatorTapsPerPhase, 0, 'f', 1));
    ui->tapsPerPhase->setValue((int)(m_settings.m_interpolatorTapsPerPhase*10.0f));
    // Enable these controls only for developers
    if (1)
    {
        ui->phaseStepsText->setVisible(false);
        ui->phaseSteps->setVisible(false);
        ui->tapsPerPhaseText->setVisible(false);
        ui->tapsPerPhase->setVisible(false);
    }
    ui->feed->setChecked(m_settings.m_feedEnabled);

    ui->flightPaths->setChecked(m_settings.m_flightPaths);
    m_aircraftModel.setFlightPaths(m_settings.m_flightPaths);
    ui->allFlightPaths->setChecked(m_settings.m_allFlightPaths);
    m_aircraftModel.setAllFlightPaths(m_settings.m_allFlightPaths);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    updateIndexLabel();

    QFont font(m_settings.m_tableFontName, m_settings.m_tableFontSize);
    ui->adsbData->setFont(font);

    // Set units in column headers
    if (m_settings.m_siUnits)
    {
        ui->adsbData->horizontalHeaderItem(ADSB_COL_ALTITUDE)->setText("Alt (m)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_SPEED)->setText("Spd (kph)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_VERTICALRATE)->setText("VR (m/s)");
    }
    else
    {
        ui->adsbData->horizontalHeaderItem(ADSB_COL_ALTITUDE)->setText("Alt (ft)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_SPEED)->setText("Spd (kn)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_VERTICALRATE)->setText("VR (ft/m)");
    }

    // Order and size columns
    QHeaderView *header = ui->adsbData->horizontalHeader();
    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->adsbData->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    // Only update airports on map if settings have changed
    if ((m_airportInfo != nullptr)
        && ((m_settings.m_airportRange != m_currentAirportRange)
            || (m_settings.m_airportMinimumSize != m_currentAirportMinimumSize)
            || (m_settings.m_displayHeliports != m_currentDisplayHeliports)))
        updateAirports();

    updateAirspaces();
    updateNavAids();

    if (!m_settings.m_displayDemodStats)
        ui->stats->setText("");

    initFlightInformation();
    initAviationWeather();

    applyMapSettings();
    applyImportSettings();

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void ADSBDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ADSBDemodGUI::enterEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ADSBDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ADSBDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_adsbDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    m_tickCount++;

    // Tick is called 20x a second - lets check this every 10 seconds
    if (m_tickCount % (20*10) == 0)
    {
        // Remove aircraft that haven't been heard of for a minute as probably out of range
        QDateTime now = QDateTime::currentDateTime();
        qint64 nowSecs = now.toSecsSinceEpoch();
        QHash<int, Aircraft *>::iterator i = m_aircraft.begin();

        while (i != m_aircraft.end())
        {
            Aircraft *aircraft = i.value();
            qint64 secondsSinceLastFrame = nowSecs - aircraft->m_time.toSecsSinceEpoch();

            if (secondsSinceLastFrame >= m_settings.m_removeTimeout)
            {
                // Don't try to track it anymore
                if (m_trackAircraft == aircraft)
                {
                    m_adsbDemod->clearTarget();
                    m_trackAircraft = nullptr;
                }

                // Remove map model
                m_aircraftModel.removeAircraft(aircraft);
                // Remove row from table
                ui->adsbData->removeRow(aircraft->m_icaoItem->row());
                // Remove aircraft from hash
                i = m_aircraft.erase(i);
                // Remove from map feature
                QList<ObjectPipe*> mapPipes;
                MainCore::instance()->getMessagePipes().getMessagePipes(this, "mapitems", mapPipes);

                for (const auto& pipe : mapPipes)
                {
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
                    swgMapItem->setName(new QString(QString("%1").arg(aircraft->m_icao, 0, 16)));
                    swgMapItem->setImage(new QString(""));
                    MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_adsbDemod, swgMapItem);
                    messageQueue->push(msg);
                }

                // And finally free its memory
                delete aircraft;
            }
            else
            {
                ++i;
            }
        }
    }
}

void ADSBDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->adsbData->rowCount();
    ui->adsbData->setRowCount(row + 1);
    ui->adsbData->setItem(row, ADSB_COL_ICAO, new QTableWidgetItem("ICAO ID"));
    ui->adsbData->setItem(row, ADSB_COL_CALLSIGN, new QTableWidgetItem("Callsign"));
    ui->adsbData->setItem(row, ADSB_COL_MODEL, new QTableWidgetItem("Aircraft12345"));
    ui->adsbData->setItem(row, ADSB_COL_AIRLINE, new QTableWidgetItem("airbrigdecargo1"));
    ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, new QTableWidgetItem("Alt (ft)"));
    ui->adsbData->setItem(row, ADSB_COL_SPEED, new QTableWidgetItem("Spd (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_HEADING, new QTableWidgetItem("Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, new QTableWidgetItem("VR (ft/m)"));
    ui->adsbData->setItem(row, ADSB_COL_RANGE, new QTableWidgetItem("D (km)"));
    ui->adsbData->setItem(row, ADSB_COL_AZEL, new QTableWidgetItem("Az/El (o)"));
    ui->adsbData->setItem(row, ADSB_COL_LATITUDE, new QTableWidgetItem("-90.00000"));
    ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, new QTableWidgetItem("-180.000000"));
    ui->adsbData->setItem(row, ADSB_COL_CATEGORY, new QTableWidgetItem("Heavy"));
    ui->adsbData->setItem(row, ADSB_COL_STATUS, new QTableWidgetItem("No emergency"));
    ui->adsbData->setItem(row, ADSB_COL_SQUAWK, new QTableWidgetItem("Squawk"));
    ui->adsbData->setItem(row, ADSB_COL_REGISTRATION, new QTableWidgetItem("G-12345"));
    ui->adsbData->setItem(row, ADSB_COL_COUNTRY, new QTableWidgetItem("Country"));
    ui->adsbData->setItem(row, ADSB_COL_REGISTERED, new QTableWidgetItem("Registered"));
    ui->adsbData->setItem(row, ADSB_COL_MANUFACTURER, new QTableWidgetItem("The Boeing Company"));
    ui->adsbData->setItem(row, ADSB_COL_OWNER, new QTableWidgetItem("British Airways"));
    ui->adsbData->setItem(row, ADSB_COL_OPERATOR_ICAO, new QTableWidgetItem("Operator"));
    ui->adsbData->setItem(row, ADSB_COL_TIME, new QTableWidgetItem("99:99:99"));
    ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, new QTableWidgetItem("Frames"));
    ui->adsbData->setItem(row, ADSB_COL_CORRELATION, new QTableWidgetItem("0.001/0.001/0.001"));
    ui->adsbData->setItem(row, ADSB_COL_RSSI, new QTableWidgetItem("-100.0"));
    ui->adsbData->setItem(row, ADSB_COL_FLIGHT_STATUS, new QTableWidgetItem("scheduled"));
    ui->adsbData->setItem(row, ADSB_COL_DEP, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_ARR, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_STD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_ETD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_ATD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_STA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_ETA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_ATA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->resizeColumnsToContents();
    ui->adsbData->setRowCount(row);
}

Aircraft* ADSBDemodGUI::findAircraftByFlight(const QString& flight)
{
    QHash<int, Aircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        Aircraft *aircraft = i.value();
        if (aircraft->m_flight == flight) {
            return aircraft;
        }
        ++i;
    }
    return nullptr;
}

// Convert to hh:mm (+/-days)
QString ADSBDemodGUI::dataTimeToShortString(QDateTime dt)
{
    if (dt.isValid())
    {
        QDate currentDate = QDateTime::currentDateTimeUtc().date();
        if (dt.date() == currentDate)
        {
            return dt.time().toString("hh:mm");
        }
        else
        {
            int days = currentDate.daysTo(dt.date());
            if (days >= 0) {
                return QString("%1 +%2").arg(dt.time().toString("hh:mm")).arg(days);
            } else {
                return QString("%1 %2").arg(dt.time().toString("hh:mm")).arg(days);
            }
        }
    }
    else
    {
        return "";
    }
}

void ADSBDemodGUI::initFlightInformation()
{
    if (m_flightInformation)
    {
        disconnect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodGUI::flightInformationUpdated);
        delete m_flightInformation;
        m_flightInformation = nullptr;
    }
    if (!m_settings.m_aviationstackAPIKey.isEmpty())
    {
        m_flightInformation = FlightInformation::create(m_settings.m_aviationstackAPIKey);
        if (m_flightInformation) {
            connect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodGUI::flightInformationUpdated);
        }
    }
}

void ADSBDemodGUI::flightInformationUpdated(const FlightInformation::Flight& flight)
{
    Aircraft* aircraft = findAircraftByFlight(flight.m_flightICAO);
    if (aircraft)
    {
        aircraft->m_flightStatusItem->setText(flight.m_flightStatus);
        aircraft->m_depItem->setText(flight.m_departureICAO);
        aircraft->m_arrItem->setText(flight.m_arrivalICAO);
        aircraft->m_stdItem->setText(dataTimeToShortString(flight.m_departureScheduled));
        aircraft->m_etdItem->setText(dataTimeToShortString(flight.m_departureEstimated));
        aircraft->m_atdItem->setText(dataTimeToShortString(flight.m_departureActual));
        aircraft->m_staItem->setText(dataTimeToShortString(flight.m_arrivalScheduled));
        aircraft->m_etaItem->setText(dataTimeToShortString(flight.m_arrivalEstimated));
        aircraft->m_ataItem->setText(dataTimeToShortString(flight.m_arrivalActual));
        if (aircraft->m_positionValid) {
            m_aircraftModel.aircraftUpdated(aircraft);
        }
        updatePhotoFlightInformation(aircraft);
    }
    else
    {
        qDebug() << "ADSBDemodGUI::flightInformationUpdated - Flight not found in ADS-B table: " << flight.m_flightICAO;
    }
}

void ADSBDemodGUI::aircraftPhoto(const PlaneSpottersPhoto *photo)
{
    // Make sure the photo is for the currently highlighted aircraft, as it may
    // have taken a while to download
    if (!photo->m_pixmap.isNull() && m_highlightAircraft && (m_highlightAircraft->m_icaoItem->text() == photo->m_icao))
    {
        ui->photo->setPixmap(photo->m_pixmap);
        ui->photo->setToolTip(QString("Photographer: %1").arg(photo->m_photographer)); // Required by terms of use
        ui->photoHeader->setVisible(true);
        ui->photoFlag->setVisible(true);
        ui->photo->setVisible(true);
        ui->flightDetails->setVisible(true);
        ui->aircraftDetails->setVisible(true);
        m_photoLink = photo->m_link;
    }
}

void ADSBDemodGUI::photoClicked()
{
    // Photo needs to link back to PlaneSpotters, as per terms of use
    if (m_highlightAircraft)
    {
        if (m_photoLink.isEmpty())
        {
            QString icaoUpper = QString("%1").arg(m_highlightAircraft->m_icao, 1, 16).toUpper();
            QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
        }
        else
        {
            QDesktopServices::openUrl(QUrl(m_photoLink));
        }
    }
}

void ADSBDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void ADSBDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySettings();
        }
    }
}

// Read .csv log and process as received frames
void ADSBDemodGUI::on_logOpen_clicked()
{
    QFileDialog fileDialog(nullptr, "Select .csv log file to read", "", "*.csv");
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Data", "Correlation"}, error);
                if (error.isEmpty())
                {
                    int dataCol = colIndexes.value("Data");
                    int correlationCol = colIndexes.value("Correlation");
                    int maxCol = std::max(dataCol, correlationCol);

                    QMessageBox dialog(this);
                    dialog.setText("Reading ADS-B data");
                    dialog.addButton(QMessageBox::Cancel);
                    dialog.show();
                    QApplication::processEvents();
                    int count = 0;
                    bool cancelled = false;
                    QStringList cols;
                    while (!cancelled && CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QDateTime dateTime = QDateTime::currentDateTime(); // So they aren't removed immediately as too old
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            float correlation = cols[correlationCol].toFloat();
                            handleADSB(bytes, dateTime, correlation, correlation, false);
                            if ((count > 0) && (count % 100000 == 0))
                            {
                                dialog.setText(QString("Reading ADS-B data\n%1").arg(count));
                                QApplication::processEvents();
                                if (dialog.clickedButton()) {
                                    cancelled = true;
                                }
                            }
                            count++;
                        }
                    }
                    m_aircraftModel.allAircraftUpdated();
                    dialog.close();
                }
                else
                {
                    QMessageBox::critical(this, "ADS-B", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "ADS-B", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void ADSBDemodGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void ADSBDemodGUI::downloadError(const QString& error)
{
    QMessageBox::critical(this, "ADS-B", error);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAirspaceFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading airspaces.");
    }
    m_airspaces = OpenAIP::readAirspaces();
    updateAirspaces();
    m_openAIP.downloadNavAids();
}

void ADSBDemodGUI::downloadNavAidsFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading NAVAIDs.");
    }
    m_navAids = OpenAIP::readNavAids();
    updateNavAids();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

int ADSBDemodGUI::grayToBinary(int gray, int bits) const
{
    int binary = 0;
    for (int i = bits - 1; i >= 0; i--) {
        binary = binary | ((((1 << (i+1)) & binary) >> 1) ^ ((1 << i) & gray));
    }
    return binary;
}

void ADSBDemodGUI::redrawMap()
{
    // An awful workaround for https://bugreports.qt.io/browse/QTBUG-100333
    // Also used in Map feature
    QQuickItem *item = ui->map->rootObject();
    if (item)
    {
        QObject *object = item->findChild<QObject*>("map");
        if (object)
        {
            double zoom = object->property("zoomLevel").value<double>();
            object->setProperty("zoomLevel", QVariant::fromValue(zoom+1));
            object->setProperty("zoomLevel", QVariant::fromValue(zoom));
        }
    }
}

void ADSBDemodGUI::showEvent(QShowEvent *event)
{
    if (!event->spontaneous())
    {
        // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
        // MapQuickItems can be in wrong position when window is first displayed
        m_redrawMapTimer.start(500);
    }
}

bool ADSBDemodGUI::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->map)
    {
        if (event->type() == QEvent::Resize)
        {
            // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
            // MapQuickItems can be in wrong position after vertical resize
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            QSize oldSize = resizeEvent->oldSize();
            QSize size = resizeEvent->size();
            if (oldSize.height() != size.height()) {
                redrawMap();
            }
        }
    }
    return false;
}

void ADSBDemodGUI::applyImportSettings()
{
    m_importTimer.setInterval(m_settings.m_importPeriod * 1000);
    if (m_settings.m_feedEnabled && m_settings.m_importEnabled) {
        m_importTimer.start();
    } else {
        m_importTimer.stop();
    }
}

// Import ADS-B data from opensky-network via an API call
void ADSBDemodGUI::import()
{
    QString urlString = "https://";
    if (!m_settings.m_importUsername.isEmpty() && !m_settings.m_importPassword.isEmpty()) {
        urlString = urlString + m_settings.m_importUsername + ":" + m_settings.m_importPassword + "@";
    }
    urlString = urlString + m_settings.m_importHost + "/api/states/all";
    QChar join = '?';
    if (!m_settings.m_importParameters.isEmpty())
    {
        urlString = urlString + join + m_settings.m_importParameters;
        join = '&';
    }
    if (!m_settings.m_importMinLatitude.isEmpty())
    {
        urlString = urlString + join + "lamin=" + m_settings.m_importMinLatitude;
        join = '&';
    }
    if (!m_settings.m_importMaxLatitude.isEmpty())
    {
        urlString = urlString + join + "lamax=" + m_settings.m_importMaxLatitude;
        join = '&';
    }
    if (!m_settings.m_importMinLongitude.isEmpty())
    {
        urlString = urlString + join + "lomin=" + m_settings.m_importMinLongitude;
        join = '&';
    }
    if (!m_settings.m_importMaxLongitude.isEmpty())
    {
        urlString = urlString + join + "lomax=" + m_settings.m_importMaxLongitude;
        join = '&';
    }
    m_networkManager->get(QNetworkRequest(QUrl(urlString)));
}

// Handle opensky-network API call reply
void ADSBDemodGUI::handleImportReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
            {
                QJsonObject obj = document.object();
                if (obj.contains("time") && obj.contains("states"))
                {
                    int seconds = obj.value("time").toInt();
                    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(seconds);
                    QJsonArray states = obj.value("states").toArray();
                    for (int i = 0; i < states.size(); i++)
                    {
                        QJsonArray state = states[i].toArray();
                        int icao = state[0].toString().toInt(nullptr, 16);

                        bool newAircraft;
                        Aircraft *aircraft = getAircraft(icao, newAircraft);

                        QString callsign = state[1].toString().trimmed();
                        if (!callsign.isEmpty())
                        {
                            aircraft->m_callsign = callsign;
                            aircraft->m_callsignItem->setText(aircraft->m_callsign);
                        }

                        QDateTime timePosition = dateTime;
                        if (state[3].isNull()) {
                            timePosition = dateTime.addSecs(-15); // At least 15 seconds old
                        } else {
                            timePosition = QDateTime::fromSecsSinceEpoch(state[3].toInt());
                        }
                        aircraft->m_time = QDateTime::fromSecsSinceEpoch(state[4].toInt());
                        QTime time = aircraft->m_time.time();
                        aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
                        aircraft->m_adsbFrameCount++;
                        aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);

                        if (timePosition > aircraft->m_positionDateTime)
                        {
                            if (!state[5].isNull() && !state[6].isNull())
                            {
                                aircraft->m_longitude = state[5].toDouble();
                                aircraft->m_latitude = state[6].toDouble();
                                aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                                aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
                                updatePosition(aircraft);
                                aircraft->m_cprValid[0] = false;
                                aircraft->m_cprValid[1] = false;
                            }
                            if (!state[7].isNull())
                            {
                                aircraft->m_altitude = (int)Units::metresToFeet(state[7].toDouble());
                                aircraft->m_altitudeValid = true;
                                aircraft->m_altitudeGNSS = false;
                                aircraft->m_altitudeItem->setData(Qt::DisplayRole, aircraft->m_altitude);
                            }
                            aircraft->m_positionDateTime = timePosition;
                        }
                        aircraft->m_onSurface = state[8].toBool(false);
                        if (!state[9].isNull())
                        {
                            aircraft->m_speed = (int)state[9].toDouble();
                            aircraft->m_speedItem->setData(Qt::DisplayRole, aircraft->m_speed);
                            aircraft->m_speedValid = true;
                            aircraft->m_speedType = Aircraft::GS;
                        }
                        if (!state[10].isNull())
                        {
                            aircraft->m_heading = (float)state[10].toDouble();
                            aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                            aircraft->m_headingValid = true;
                            aircraft->m_headingDateTime = aircraft->m_time;
                        }
                        if (!state[11].isNull())
                        {
                            aircraft->m_verticalRate = (int)state[10].toDouble();
                            aircraft->m_verticalRateItem->setData(Qt::DisplayRole, aircraft->m_verticalRate);
                            aircraft->m_verticalRateValid = true;
                        }
                        if (!state[14].isNull())
                        {
                            aircraft->m_squawk = state[14].toString().toInt();
                            aircraft->m_squawkItem->setText(QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                        }

                        // Update aircraft in map
                        if (aircraft->m_positionValid)
                        {
                            // Check to see if we need to start any animations
                            QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);

                            // Update map displayed in channel
                            m_aircraftModel.aircraftUpdated(aircraft);

                            // Send to Map feature
                            sendToMap(aircraft, animations);
                        }

                        // Check to see if we need to emit a notification about this aircraft
                        checkDynamicNotification(aircraft);
                    }
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::handleImportReply: Document object does not contain time and states: " << document;
                }
            }
            else
            {
                qDebug() << "ADSBDemodGUI::handleImportReply: Document is not an object: " << document;
            }
        }
        else
        {
            qDebug() << "ADSBDemodGUI::handleImportReply: error " << reply->error();
        }
        reply->deleteLater();
    }
}

void ADSBDemodGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
        Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
        Real stationAltitude = MainCore::instance()->getSettings().getAltitude();

        if (   (stationLatitude != m_azEl.getLocationSpherical().m_latitude)
            || (stationLongitude != m_azEl.getLocationSpherical().m_longitude)
            || (stationAltitude != m_azEl.getLocationSpherical().m_altitude))
        {
            m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

            // Update distances and what is visible
            updateAirports();
            updateAirspaces();
            updateNavAids();

            // Update icon position on Map
            QQuickItem *item = ui->map->rootObject();
            QObject *map = item->findChild<QObject*>("map");
            if (map != nullptr)
            {
                QObject *stationObject = map->findChild<QObject*>("station");
                if(stationObject != NULL)
                {
                    QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                    coords.setLatitude(stationLatitude);
                    coords.setLongitude(stationLongitude);
                    coords.setAltitude(stationAltitude);
                    stationObject->setProperty("coordinate", QVariant::fromValue(coords));
                }
            }
        }
    }
    if (pref == Preferences::StationName)
    {
        // Update icon label on Map
        QQuickItem *item = ui->map->rootObject();
        QObject *map = item->findChild<QObject*>("map");
        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");
            if(stationObject != NULL) {
                stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
            }
        }
    }
}

void ADSBDemodGUI::initAviationWeather()
{
    if (m_aviationWeather)
    {
        disconnect(m_aviationWeather, &AviationWeather::weatherUpdated, this, &ADSBDemodGUI::weatherUpdated);
        delete m_aviationWeather;
        m_aviationWeather = nullptr;
    }
    if (!m_settings.m_checkWXAPIKey.isEmpty())
    {
        m_aviationWeather = AviationWeather::create(m_settings.m_checkWXAPIKey);
        if (m_aviationWeather) {
            connect(m_aviationWeather, &AviationWeather::weatherUpdated, this, &ADSBDemodGUI::weatherUpdated);
        }
    }
}

void ADSBDemodGUI::requestMetar(const QString& icao)
{
    if (m_aviationWeather) {
        m_aviationWeather->getWeather(icao);
    }
}

void ADSBDemodGUI::weatherUpdated(const AviationWeather::METAR &metar)
{
    m_airportModel.updateWeather(metar.m_icao, metar.m_text, metar.decoded());
}

void ADSBDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ADSBDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &ADSBDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &ADSBDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->phaseSteps, &QDial::valueChanged, this, &ADSBDemodGUI::on_phaseSteps_valueChanged);
    QObject::connect(ui->tapsPerPhase, &QDial::valueChanged, this, &ADSBDemodGUI::on_tapsPerPhase_valueChanged);
    QObject::connect(ui->adsbData, &QTableWidget::cellClicked, this, &ADSBDemodGUI::on_adsbData_cellClicked);
    QObject::connect(ui->adsbData, &QTableWidget::cellDoubleClicked, this, &ADSBDemodGUI::on_adsbData_cellDoubleClicked);
    QObject::connect(ui->spb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_spb_currentIndexChanged);
    QObject::connect(ui->correlateFullPreamble, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_correlateFullPreamble_clicked);
    QObject::connect(ui->demodModeS, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_demodModeS_clicked);
    QObject::connect(ui->feed, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_feed_clicked);
    QObject::connect(ui->notifications, &QToolButton::clicked, this, &ADSBDemodGUI::on_notifications_clicked);
    QObject::connect(ui->flightInfo, &QToolButton::clicked, this, &ADSBDemodGUI::on_flightInfo_clicked);
    QObject::connect(ui->findOnMapFeature, &QToolButton::clicked, this, &ADSBDemodGUI::on_findOnMapFeature_clicked);
    QObject::connect(ui->getOSNDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getOSNDB_clicked);
    QObject::connect(ui->getAirportDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirportDB_clicked);
    QObject::connect(ui->getAirspacesDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirspacesDB_clicked);
    QObject::connect(ui->flightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_flightPaths_clicked);
    QObject::connect(ui->allFlightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_allFlightPaths_clicked);
    QObject::connect(ui->device, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_device_currentIndexChanged);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &ADSBDemodGUI::on_displaySettings_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &ADSBDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &ADSBDemodGUI::on_logOpen_clicked);
}

void ADSBDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
