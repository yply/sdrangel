///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_REMOTETCPSINKBASEBAND_H
#define INCLUDE_REMOTETCPSINKBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "remotetcpsinksink.h"
#include "remotetcpsinksettings.h"

class DownChannelizer;

class RemoteTCPSinkBaseband : public QObject
{
    Q_OBJECT
public:
    RemoteTCPSinkBaseband();
    ~RemoteTCPSinkBaseband();

    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_sink.setMessageQueueToGUI(messageQueue); }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    void setDeviceIndex(uint32_t deviceIndex) { m_sink.setDeviceIndex(deviceIndex); }
    void setChannelIndex(uint32_t channelIndex) { m_sink.setChannelIndex(channelIndex); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    bool m_running;
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    RemoteTCPSinkSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    RemoteTCPSinkSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const RemoteTCPSinkSettings& settings, bool force = false, bool remoteChange = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_REMOTETCPSINKBASEBAND_H
