///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_SDRDAEMONGUI_H
#define INCLUDE_SDRDAEMONGUI_H

#include <QTimer>
#include "plugin/plugingui.h"

#include "sdrdaemoninput.h"

class PluginAPI;

namespace Ui {
	class SDRdaemonGui;
}

class SDRdaemonGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit SDRdaemonGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~SDRdaemonGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual bool handleMessage(const Message& message);

private:
	Ui::SDRdaemonGui* ui;

	PluginAPI* m_pluginAPI;
	SDRdaemonInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;
    bool m_acquisition;
    QString m_fileName;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	int m_samplesCount;
	std::size_t m_tickCount;

	void displaySettings();
	void displayTime();
	void sendSettings();
	void updateHardware();
	void configureFileName();
	void updateWithAcquisition();
	void updateWithStreamData();
	void updateWithStreamTime();

private slots:
	void handleSourceMessages();
	void on_playLoop_toggled(bool checked);
	void on_play_toggled(bool checked);
	void on_showFileDialog_clicked(bool checked);
	void tick();
};

#endif // INCLUDE_SDRDAEMONGUI_H
