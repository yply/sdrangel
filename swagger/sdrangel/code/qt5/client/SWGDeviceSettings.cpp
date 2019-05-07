/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube    ---   Limitations and specifcities:    * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV and DATV demodulators, Channel Analyzer NG, LoRa demodulator   * The device settings and report structures contains only the sub-structure corresponding to the device type. The DeviceSettings and DeviceReport structures documented here shows all of them but only one will be or should be present at a time   * The channel settings and report structures contains only the sub-structure corresponding to the channel type. The ChannelSettings and ChannelReport structures documented here shows all of them but only one will be or should be present at a time    --- 
 *
 * OpenAPI spec version: 4.7.1
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGDeviceSettings.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGDeviceSettings::SWGDeviceSettings(QString* json) {
    init();
    this->fromJson(*json);
}

SWGDeviceSettings::SWGDeviceSettings() {
    device_hw_type = nullptr;
    m_device_hw_type_isSet = false;
    direction = 0;
    m_direction_isSet = false;
    originator_index = 0;
    m_originator_index_isSet = false;
    airspy_settings = nullptr;
    m_airspy_settings_isSet = false;
    airspy_hf_settings = nullptr;
    m_airspy_hf_settings_isSet = false;
    blade_rf1_input_settings = nullptr;
    m_blade_rf1_input_settings_isSet = false;
    blade_rf2_input_settings = nullptr;
    m_blade_rf2_input_settings_isSet = false;
    blade_rf1_output_settings = nullptr;
    m_blade_rf1_output_settings_isSet = false;
    blade_rf2_output_settings = nullptr;
    m_blade_rf2_output_settings_isSet = false;
    fcd_pro_settings = nullptr;
    m_fcd_pro_settings_isSet = false;
    fcd_pro_plus_settings = nullptr;
    m_fcd_pro_plus_settings_isSet = false;
    file_source_settings = nullptr;
    m_file_source_settings_isSet = false;
    hack_rf_input_settings = nullptr;
    m_hack_rf_input_settings_isSet = false;
    hack_rf_output_settings = nullptr;
    m_hack_rf_output_settings_isSet = false;
    lime_sdr_input_settings = nullptr;
    m_lime_sdr_input_settings_isSet = false;
    lime_sdr_output_settings = nullptr;
    m_lime_sdr_output_settings_isSet = false;
    local_input_settings = nullptr;
    m_local_input_settings_isSet = false;
    perseus_settings = nullptr;
    m_perseus_settings_isSet = false;
    pluto_sdr_input_settings = nullptr;
    m_pluto_sdr_input_settings_isSet = false;
    pluto_sdr_output_settings = nullptr;
    m_pluto_sdr_output_settings_isSet = false;
    rtl_sdr_settings = nullptr;
    m_rtl_sdr_settings_isSet = false;
    remote_output_settings = nullptr;
    m_remote_output_settings_isSet = false;
    remote_input_settings = nullptr;
    m_remote_input_settings_isSet = false;
    sdr_play_settings = nullptr;
    m_sdr_play_settings_isSet = false;
    soapy_sdr_input_settings = nullptr;
    m_soapy_sdr_input_settings_isSet = false;
    soapy_sdr_output_settings = nullptr;
    m_soapy_sdr_output_settings_isSet = false;
    test_source_settings = nullptr;
    m_test_source_settings_isSet = false;
    xtrx_input_settings = nullptr;
    m_xtrx_input_settings_isSet = false;
    xtrx_output_settings = nullptr;
    m_xtrx_output_settings_isSet = false;
}

SWGDeviceSettings::~SWGDeviceSettings() {
    this->cleanup();
}

void
SWGDeviceSettings::init() {
    device_hw_type = new QString("");
    m_device_hw_type_isSet = false;
    direction = 0;
    m_direction_isSet = false;
    originator_index = 0;
    m_originator_index_isSet = false;
    airspy_settings = new SWGAirspySettings();
    m_airspy_settings_isSet = false;
    airspy_hf_settings = new SWGAirspyHFSettings();
    m_airspy_hf_settings_isSet = false;
    blade_rf1_input_settings = new SWGBladeRF1InputSettings();
    m_blade_rf1_input_settings_isSet = false;
    blade_rf2_input_settings = new SWGBladeRF2InputSettings();
    m_blade_rf2_input_settings_isSet = false;
    blade_rf1_output_settings = new SWGBladeRF1OutputSettings();
    m_blade_rf1_output_settings_isSet = false;
    blade_rf2_output_settings = new SWGBladeRF2OutputSettings();
    m_blade_rf2_output_settings_isSet = false;
    fcd_pro_settings = new SWGFCDProSettings();
    m_fcd_pro_settings_isSet = false;
    fcd_pro_plus_settings = new SWGFCDProPlusSettings();
    m_fcd_pro_plus_settings_isSet = false;
    file_source_settings = new SWGFileSourceSettings();
    m_file_source_settings_isSet = false;
    hack_rf_input_settings = new SWGHackRFInputSettings();
    m_hack_rf_input_settings_isSet = false;
    hack_rf_output_settings = new SWGHackRFOutputSettings();
    m_hack_rf_output_settings_isSet = false;
    lime_sdr_input_settings = new SWGLimeSdrInputSettings();
    m_lime_sdr_input_settings_isSet = false;
    lime_sdr_output_settings = new SWGLimeSdrOutputSettings();
    m_lime_sdr_output_settings_isSet = false;
    local_input_settings = new SWGLocalInputSettings();
    m_local_input_settings_isSet = false;
    perseus_settings = new SWGPerseusSettings();
    m_perseus_settings_isSet = false;
    pluto_sdr_input_settings = new SWGPlutoSdrInputSettings();
    m_pluto_sdr_input_settings_isSet = false;
    pluto_sdr_output_settings = new SWGPlutoSdrOutputSettings();
    m_pluto_sdr_output_settings_isSet = false;
    rtl_sdr_settings = new SWGRtlSdrSettings();
    m_rtl_sdr_settings_isSet = false;
    remote_output_settings = new SWGRemoteOutputSettings();
    m_remote_output_settings_isSet = false;
    remote_input_settings = new SWGRemoteInputSettings();
    m_remote_input_settings_isSet = false;
    sdr_play_settings = new SWGSDRPlaySettings();
    m_sdr_play_settings_isSet = false;
    soapy_sdr_input_settings = new SWGSoapySDRInputSettings();
    m_soapy_sdr_input_settings_isSet = false;
    soapy_sdr_output_settings = new SWGSoapySDROutputSettings();
    m_soapy_sdr_output_settings_isSet = false;
    test_source_settings = new SWGTestSourceSettings();
    m_test_source_settings_isSet = false;
    xtrx_input_settings = new SWGXtrxInputSettings();
    m_xtrx_input_settings_isSet = false;
    xtrx_output_settings = new SWGXtrxOutputSettings();
    m_xtrx_output_settings_isSet = false;
}

void
SWGDeviceSettings::cleanup() {
    if(device_hw_type != nullptr) { 
        delete device_hw_type;
    }


    if(airspy_settings != nullptr) { 
        delete airspy_settings;
    }
    if(airspy_hf_settings != nullptr) { 
        delete airspy_hf_settings;
    }
    if(blade_rf1_input_settings != nullptr) { 
        delete blade_rf1_input_settings;
    }
    if(blade_rf2_input_settings != nullptr) { 
        delete blade_rf2_input_settings;
    }
    if(blade_rf1_output_settings != nullptr) { 
        delete blade_rf1_output_settings;
    }
    if(blade_rf2_output_settings != nullptr) { 
        delete blade_rf2_output_settings;
    }
    if(fcd_pro_settings != nullptr) { 
        delete fcd_pro_settings;
    }
    if(fcd_pro_plus_settings != nullptr) { 
        delete fcd_pro_plus_settings;
    }
    if(file_source_settings != nullptr) { 
        delete file_source_settings;
    }
    if(hack_rf_input_settings != nullptr) { 
        delete hack_rf_input_settings;
    }
    if(hack_rf_output_settings != nullptr) { 
        delete hack_rf_output_settings;
    }
    if(lime_sdr_input_settings != nullptr) { 
        delete lime_sdr_input_settings;
    }
    if(lime_sdr_output_settings != nullptr) { 
        delete lime_sdr_output_settings;
    }
    if(local_input_settings != nullptr) { 
        delete local_input_settings;
    }
    if(perseus_settings != nullptr) { 
        delete perseus_settings;
    }
    if(pluto_sdr_input_settings != nullptr) { 
        delete pluto_sdr_input_settings;
    }
    if(pluto_sdr_output_settings != nullptr) { 
        delete pluto_sdr_output_settings;
    }
    if(rtl_sdr_settings != nullptr) { 
        delete rtl_sdr_settings;
    }
    if(remote_output_settings != nullptr) { 
        delete remote_output_settings;
    }
    if(remote_input_settings != nullptr) { 
        delete remote_input_settings;
    }
    if(sdr_play_settings != nullptr) { 
        delete sdr_play_settings;
    }
    if(soapy_sdr_input_settings != nullptr) { 
        delete soapy_sdr_input_settings;
    }
    if(soapy_sdr_output_settings != nullptr) { 
        delete soapy_sdr_output_settings;
    }
    if(test_source_settings != nullptr) { 
        delete test_source_settings;
    }
    if(xtrx_input_settings != nullptr) { 
        delete xtrx_input_settings;
    }
    if(xtrx_output_settings != nullptr) { 
        delete xtrx_output_settings;
    }
}

SWGDeviceSettings*
SWGDeviceSettings::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGDeviceSettings::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&device_hw_type, pJson["deviceHwType"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&direction, pJson["direction"], "qint32", "");
    
    ::SWGSDRangel::setValue(&originator_index, pJson["originatorIndex"], "qint32", "");
    
    ::SWGSDRangel::setValue(&airspy_settings, pJson["airspySettings"], "SWGAirspySettings", "SWGAirspySettings");
    
    ::SWGSDRangel::setValue(&airspy_hf_settings, pJson["airspyHFSettings"], "SWGAirspyHFSettings", "SWGAirspyHFSettings");
    
    ::SWGSDRangel::setValue(&blade_rf1_input_settings, pJson["bladeRF1InputSettings"], "SWGBladeRF1InputSettings", "SWGBladeRF1InputSettings");
    
    ::SWGSDRangel::setValue(&blade_rf2_input_settings, pJson["bladeRF2InputSettings"], "SWGBladeRF2InputSettings", "SWGBladeRF2InputSettings");
    
    ::SWGSDRangel::setValue(&blade_rf1_output_settings, pJson["bladeRF1OutputSettings"], "SWGBladeRF1OutputSettings", "SWGBladeRF1OutputSettings");
    
    ::SWGSDRangel::setValue(&blade_rf2_output_settings, pJson["bladeRF2OutputSettings"], "SWGBladeRF2OutputSettings", "SWGBladeRF2OutputSettings");
    
    ::SWGSDRangel::setValue(&fcd_pro_settings, pJson["fcdProSettings"], "SWGFCDProSettings", "SWGFCDProSettings");
    
    ::SWGSDRangel::setValue(&fcd_pro_plus_settings, pJson["fcdProPlusSettings"], "SWGFCDProPlusSettings", "SWGFCDProPlusSettings");
    
    ::SWGSDRangel::setValue(&file_source_settings, pJson["fileSourceSettings"], "SWGFileSourceSettings", "SWGFileSourceSettings");
    
    ::SWGSDRangel::setValue(&hack_rf_input_settings, pJson["hackRFInputSettings"], "SWGHackRFInputSettings", "SWGHackRFInputSettings");
    
    ::SWGSDRangel::setValue(&hack_rf_output_settings, pJson["hackRFOutputSettings"], "SWGHackRFOutputSettings", "SWGHackRFOutputSettings");
    
    ::SWGSDRangel::setValue(&lime_sdr_input_settings, pJson["limeSdrInputSettings"], "SWGLimeSdrInputSettings", "SWGLimeSdrInputSettings");
    
    ::SWGSDRangel::setValue(&lime_sdr_output_settings, pJson["limeSdrOutputSettings"], "SWGLimeSdrOutputSettings", "SWGLimeSdrOutputSettings");
    
    ::SWGSDRangel::setValue(&local_input_settings, pJson["localInputSettings"], "SWGLocalInputSettings", "SWGLocalInputSettings");
    
    ::SWGSDRangel::setValue(&perseus_settings, pJson["perseusSettings"], "SWGPerseusSettings", "SWGPerseusSettings");
    
    ::SWGSDRangel::setValue(&pluto_sdr_input_settings, pJson["plutoSdrInputSettings"], "SWGPlutoSdrInputSettings", "SWGPlutoSdrInputSettings");
    
    ::SWGSDRangel::setValue(&pluto_sdr_output_settings, pJson["plutoSdrOutputSettings"], "SWGPlutoSdrOutputSettings", "SWGPlutoSdrOutputSettings");
    
    ::SWGSDRangel::setValue(&rtl_sdr_settings, pJson["rtlSdrSettings"], "SWGRtlSdrSettings", "SWGRtlSdrSettings");
    
    ::SWGSDRangel::setValue(&remote_output_settings, pJson["remoteOutputSettings"], "SWGRemoteOutputSettings", "SWGRemoteOutputSettings");
    
    ::SWGSDRangel::setValue(&remote_input_settings, pJson["remoteInputSettings"], "SWGRemoteInputSettings", "SWGRemoteInputSettings");
    
    ::SWGSDRangel::setValue(&sdr_play_settings, pJson["sdrPlaySettings"], "SWGSDRPlaySettings", "SWGSDRPlaySettings");
    
    ::SWGSDRangel::setValue(&soapy_sdr_input_settings, pJson["soapySDRInputSettings"], "SWGSoapySDRInputSettings", "SWGSoapySDRInputSettings");
    
    ::SWGSDRangel::setValue(&soapy_sdr_output_settings, pJson["soapySDROutputSettings"], "SWGSoapySDROutputSettings", "SWGSoapySDROutputSettings");
    
    ::SWGSDRangel::setValue(&test_source_settings, pJson["testSourceSettings"], "SWGTestSourceSettings", "SWGTestSourceSettings");
    
    ::SWGSDRangel::setValue(&xtrx_input_settings, pJson["xtrxInputSettings"], "SWGXtrxInputSettings", "SWGXtrxInputSettings");
    
    ::SWGSDRangel::setValue(&xtrx_output_settings, pJson["xtrxOutputSettings"], "SWGXtrxOutputSettings", "SWGXtrxOutputSettings");
    
}

QString
SWGDeviceSettings::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGDeviceSettings::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(device_hw_type != nullptr && *device_hw_type != QString("")){
        toJsonValue(QString("deviceHwType"), device_hw_type, obj, QString("QString"));
    }
    if(m_direction_isSet){
        obj->insert("direction", QJsonValue(direction));
    }
    if(m_originator_index_isSet){
        obj->insert("originatorIndex", QJsonValue(originator_index));
    }
    if((airspy_settings != nullptr) && (airspy_settings->isSet())){
        toJsonValue(QString("airspySettings"), airspy_settings, obj, QString("SWGAirspySettings"));
    }
    if((airspy_hf_settings != nullptr) && (airspy_hf_settings->isSet())){
        toJsonValue(QString("airspyHFSettings"), airspy_hf_settings, obj, QString("SWGAirspyHFSettings"));
    }
    if((blade_rf1_input_settings != nullptr) && (blade_rf1_input_settings->isSet())){
        toJsonValue(QString("bladeRF1InputSettings"), blade_rf1_input_settings, obj, QString("SWGBladeRF1InputSettings"));
    }
    if((blade_rf2_input_settings != nullptr) && (blade_rf2_input_settings->isSet())){
        toJsonValue(QString("bladeRF2InputSettings"), blade_rf2_input_settings, obj, QString("SWGBladeRF2InputSettings"));
    }
    if((blade_rf1_output_settings != nullptr) && (blade_rf1_output_settings->isSet())){
        toJsonValue(QString("bladeRF1OutputSettings"), blade_rf1_output_settings, obj, QString("SWGBladeRF1OutputSettings"));
    }
    if((blade_rf2_output_settings != nullptr) && (blade_rf2_output_settings->isSet())){
        toJsonValue(QString("bladeRF2OutputSettings"), blade_rf2_output_settings, obj, QString("SWGBladeRF2OutputSettings"));
    }
    if((fcd_pro_settings != nullptr) && (fcd_pro_settings->isSet())){
        toJsonValue(QString("fcdProSettings"), fcd_pro_settings, obj, QString("SWGFCDProSettings"));
    }
    if((fcd_pro_plus_settings != nullptr) && (fcd_pro_plus_settings->isSet())){
        toJsonValue(QString("fcdProPlusSettings"), fcd_pro_plus_settings, obj, QString("SWGFCDProPlusSettings"));
    }
    if((file_source_settings != nullptr) && (file_source_settings->isSet())){
        toJsonValue(QString("fileSourceSettings"), file_source_settings, obj, QString("SWGFileSourceSettings"));
    }
    if((hack_rf_input_settings != nullptr) && (hack_rf_input_settings->isSet())){
        toJsonValue(QString("hackRFInputSettings"), hack_rf_input_settings, obj, QString("SWGHackRFInputSettings"));
    }
    if((hack_rf_output_settings != nullptr) && (hack_rf_output_settings->isSet())){
        toJsonValue(QString("hackRFOutputSettings"), hack_rf_output_settings, obj, QString("SWGHackRFOutputSettings"));
    }
    if((lime_sdr_input_settings != nullptr) && (lime_sdr_input_settings->isSet())){
        toJsonValue(QString("limeSdrInputSettings"), lime_sdr_input_settings, obj, QString("SWGLimeSdrInputSettings"));
    }
    if((lime_sdr_output_settings != nullptr) && (lime_sdr_output_settings->isSet())){
        toJsonValue(QString("limeSdrOutputSettings"), lime_sdr_output_settings, obj, QString("SWGLimeSdrOutputSettings"));
    }
    if((local_input_settings != nullptr) && (local_input_settings->isSet())){
        toJsonValue(QString("localInputSettings"), local_input_settings, obj, QString("SWGLocalInputSettings"));
    }
    if((perseus_settings != nullptr) && (perseus_settings->isSet())){
        toJsonValue(QString("perseusSettings"), perseus_settings, obj, QString("SWGPerseusSettings"));
    }
    if((pluto_sdr_input_settings != nullptr) && (pluto_sdr_input_settings->isSet())){
        toJsonValue(QString("plutoSdrInputSettings"), pluto_sdr_input_settings, obj, QString("SWGPlutoSdrInputSettings"));
    }
    if((pluto_sdr_output_settings != nullptr) && (pluto_sdr_output_settings->isSet())){
        toJsonValue(QString("plutoSdrOutputSettings"), pluto_sdr_output_settings, obj, QString("SWGPlutoSdrOutputSettings"));
    }
    if((rtl_sdr_settings != nullptr) && (rtl_sdr_settings->isSet())){
        toJsonValue(QString("rtlSdrSettings"), rtl_sdr_settings, obj, QString("SWGRtlSdrSettings"));
    }
    if((remote_output_settings != nullptr) && (remote_output_settings->isSet())){
        toJsonValue(QString("remoteOutputSettings"), remote_output_settings, obj, QString("SWGRemoteOutputSettings"));
    }
    if((remote_input_settings != nullptr) && (remote_input_settings->isSet())){
        toJsonValue(QString("remoteInputSettings"), remote_input_settings, obj, QString("SWGRemoteInputSettings"));
    }
    if((sdr_play_settings != nullptr) && (sdr_play_settings->isSet())){
        toJsonValue(QString("sdrPlaySettings"), sdr_play_settings, obj, QString("SWGSDRPlaySettings"));
    }
    if((soapy_sdr_input_settings != nullptr) && (soapy_sdr_input_settings->isSet())){
        toJsonValue(QString("soapySDRInputSettings"), soapy_sdr_input_settings, obj, QString("SWGSoapySDRInputSettings"));
    }
    if((soapy_sdr_output_settings != nullptr) && (soapy_sdr_output_settings->isSet())){
        toJsonValue(QString("soapySDROutputSettings"), soapy_sdr_output_settings, obj, QString("SWGSoapySDROutputSettings"));
    }
    if((test_source_settings != nullptr) && (test_source_settings->isSet())){
        toJsonValue(QString("testSourceSettings"), test_source_settings, obj, QString("SWGTestSourceSettings"));
    }
    if((xtrx_input_settings != nullptr) && (xtrx_input_settings->isSet())){
        toJsonValue(QString("xtrxInputSettings"), xtrx_input_settings, obj, QString("SWGXtrxInputSettings"));
    }
    if((xtrx_output_settings != nullptr) && (xtrx_output_settings->isSet())){
        toJsonValue(QString("xtrxOutputSettings"), xtrx_output_settings, obj, QString("SWGXtrxOutputSettings"));
    }

    return obj;
}

QString*
SWGDeviceSettings::getDeviceHwType() {
    return device_hw_type;
}
void
SWGDeviceSettings::setDeviceHwType(QString* device_hw_type) {
    this->device_hw_type = device_hw_type;
    this->m_device_hw_type_isSet = true;
}

qint32
SWGDeviceSettings::getDirection() {
    return direction;
}
void
SWGDeviceSettings::setDirection(qint32 direction) {
    this->direction = direction;
    this->m_direction_isSet = true;
}

qint32
SWGDeviceSettings::getOriginatorIndex() {
    return originator_index;
}
void
SWGDeviceSettings::setOriginatorIndex(qint32 originator_index) {
    this->originator_index = originator_index;
    this->m_originator_index_isSet = true;
}

SWGAirspySettings*
SWGDeviceSettings::getAirspySettings() {
    return airspy_settings;
}
void
SWGDeviceSettings::setAirspySettings(SWGAirspySettings* airspy_settings) {
    this->airspy_settings = airspy_settings;
    this->m_airspy_settings_isSet = true;
}

SWGAirspyHFSettings*
SWGDeviceSettings::getAirspyHfSettings() {
    return airspy_hf_settings;
}
void
SWGDeviceSettings::setAirspyHfSettings(SWGAirspyHFSettings* airspy_hf_settings) {
    this->airspy_hf_settings = airspy_hf_settings;
    this->m_airspy_hf_settings_isSet = true;
}

SWGBladeRF1InputSettings*
SWGDeviceSettings::getBladeRf1InputSettings() {
    return blade_rf1_input_settings;
}
void
SWGDeviceSettings::setBladeRf1InputSettings(SWGBladeRF1InputSettings* blade_rf1_input_settings) {
    this->blade_rf1_input_settings = blade_rf1_input_settings;
    this->m_blade_rf1_input_settings_isSet = true;
}

SWGBladeRF2InputSettings*
SWGDeviceSettings::getBladeRf2InputSettings() {
    return blade_rf2_input_settings;
}
void
SWGDeviceSettings::setBladeRf2InputSettings(SWGBladeRF2InputSettings* blade_rf2_input_settings) {
    this->blade_rf2_input_settings = blade_rf2_input_settings;
    this->m_blade_rf2_input_settings_isSet = true;
}

SWGBladeRF1OutputSettings*
SWGDeviceSettings::getBladeRf1OutputSettings() {
    return blade_rf1_output_settings;
}
void
SWGDeviceSettings::setBladeRf1OutputSettings(SWGBladeRF1OutputSettings* blade_rf1_output_settings) {
    this->blade_rf1_output_settings = blade_rf1_output_settings;
    this->m_blade_rf1_output_settings_isSet = true;
}

SWGBladeRF2OutputSettings*
SWGDeviceSettings::getBladeRf2OutputSettings() {
    return blade_rf2_output_settings;
}
void
SWGDeviceSettings::setBladeRf2OutputSettings(SWGBladeRF2OutputSettings* blade_rf2_output_settings) {
    this->blade_rf2_output_settings = blade_rf2_output_settings;
    this->m_blade_rf2_output_settings_isSet = true;
}

SWGFCDProSettings*
SWGDeviceSettings::getFcdProSettings() {
    return fcd_pro_settings;
}
void
SWGDeviceSettings::setFcdProSettings(SWGFCDProSettings* fcd_pro_settings) {
    this->fcd_pro_settings = fcd_pro_settings;
    this->m_fcd_pro_settings_isSet = true;
}

SWGFCDProPlusSettings*
SWGDeviceSettings::getFcdProPlusSettings() {
    return fcd_pro_plus_settings;
}
void
SWGDeviceSettings::setFcdProPlusSettings(SWGFCDProPlusSettings* fcd_pro_plus_settings) {
    this->fcd_pro_plus_settings = fcd_pro_plus_settings;
    this->m_fcd_pro_plus_settings_isSet = true;
}

SWGFileSourceSettings*
SWGDeviceSettings::getFileSourceSettings() {
    return file_source_settings;
}
void
SWGDeviceSettings::setFileSourceSettings(SWGFileSourceSettings* file_source_settings) {
    this->file_source_settings = file_source_settings;
    this->m_file_source_settings_isSet = true;
}

SWGHackRFInputSettings*
SWGDeviceSettings::getHackRfInputSettings() {
    return hack_rf_input_settings;
}
void
SWGDeviceSettings::setHackRfInputSettings(SWGHackRFInputSettings* hack_rf_input_settings) {
    this->hack_rf_input_settings = hack_rf_input_settings;
    this->m_hack_rf_input_settings_isSet = true;
}

SWGHackRFOutputSettings*
SWGDeviceSettings::getHackRfOutputSettings() {
    return hack_rf_output_settings;
}
void
SWGDeviceSettings::setHackRfOutputSettings(SWGHackRFOutputSettings* hack_rf_output_settings) {
    this->hack_rf_output_settings = hack_rf_output_settings;
    this->m_hack_rf_output_settings_isSet = true;
}

SWGLimeSdrInputSettings*
SWGDeviceSettings::getLimeSdrInputSettings() {
    return lime_sdr_input_settings;
}
void
SWGDeviceSettings::setLimeSdrInputSettings(SWGLimeSdrInputSettings* lime_sdr_input_settings) {
    this->lime_sdr_input_settings = lime_sdr_input_settings;
    this->m_lime_sdr_input_settings_isSet = true;
}

SWGLimeSdrOutputSettings*
SWGDeviceSettings::getLimeSdrOutputSettings() {
    return lime_sdr_output_settings;
}
void
SWGDeviceSettings::setLimeSdrOutputSettings(SWGLimeSdrOutputSettings* lime_sdr_output_settings) {
    this->lime_sdr_output_settings = lime_sdr_output_settings;
    this->m_lime_sdr_output_settings_isSet = true;
}

SWGLocalInputSettings*
SWGDeviceSettings::getLocalInputSettings() {
    return local_input_settings;
}
void
SWGDeviceSettings::setLocalInputSettings(SWGLocalInputSettings* local_input_settings) {
    this->local_input_settings = local_input_settings;
    this->m_local_input_settings_isSet = true;
}

SWGPerseusSettings*
SWGDeviceSettings::getPerseusSettings() {
    return perseus_settings;
}
void
SWGDeviceSettings::setPerseusSettings(SWGPerseusSettings* perseus_settings) {
    this->perseus_settings = perseus_settings;
    this->m_perseus_settings_isSet = true;
}

SWGPlutoSdrInputSettings*
SWGDeviceSettings::getPlutoSdrInputSettings() {
    return pluto_sdr_input_settings;
}
void
SWGDeviceSettings::setPlutoSdrInputSettings(SWGPlutoSdrInputSettings* pluto_sdr_input_settings) {
    this->pluto_sdr_input_settings = pluto_sdr_input_settings;
    this->m_pluto_sdr_input_settings_isSet = true;
}

SWGPlutoSdrOutputSettings*
SWGDeviceSettings::getPlutoSdrOutputSettings() {
    return pluto_sdr_output_settings;
}
void
SWGDeviceSettings::setPlutoSdrOutputSettings(SWGPlutoSdrOutputSettings* pluto_sdr_output_settings) {
    this->pluto_sdr_output_settings = pluto_sdr_output_settings;
    this->m_pluto_sdr_output_settings_isSet = true;
}

SWGRtlSdrSettings*
SWGDeviceSettings::getRtlSdrSettings() {
    return rtl_sdr_settings;
}
void
SWGDeviceSettings::setRtlSdrSettings(SWGRtlSdrSettings* rtl_sdr_settings) {
    this->rtl_sdr_settings = rtl_sdr_settings;
    this->m_rtl_sdr_settings_isSet = true;
}

SWGRemoteOutputSettings*
SWGDeviceSettings::getRemoteOutputSettings() {
    return remote_output_settings;
}
void
SWGDeviceSettings::setRemoteOutputSettings(SWGRemoteOutputSettings* remote_output_settings) {
    this->remote_output_settings = remote_output_settings;
    this->m_remote_output_settings_isSet = true;
}

SWGRemoteInputSettings*
SWGDeviceSettings::getRemoteInputSettings() {
    return remote_input_settings;
}
void
SWGDeviceSettings::setRemoteInputSettings(SWGRemoteInputSettings* remote_input_settings) {
    this->remote_input_settings = remote_input_settings;
    this->m_remote_input_settings_isSet = true;
}

SWGSDRPlaySettings*
SWGDeviceSettings::getSdrPlaySettings() {
    return sdr_play_settings;
}
void
SWGDeviceSettings::setSdrPlaySettings(SWGSDRPlaySettings* sdr_play_settings) {
    this->sdr_play_settings = sdr_play_settings;
    this->m_sdr_play_settings_isSet = true;
}

SWGSoapySDRInputSettings*
SWGDeviceSettings::getSoapySdrInputSettings() {
    return soapy_sdr_input_settings;
}
void
SWGDeviceSettings::setSoapySdrInputSettings(SWGSoapySDRInputSettings* soapy_sdr_input_settings) {
    this->soapy_sdr_input_settings = soapy_sdr_input_settings;
    this->m_soapy_sdr_input_settings_isSet = true;
}

SWGSoapySDROutputSettings*
SWGDeviceSettings::getSoapySdrOutputSettings() {
    return soapy_sdr_output_settings;
}
void
SWGDeviceSettings::setSoapySdrOutputSettings(SWGSoapySDROutputSettings* soapy_sdr_output_settings) {
    this->soapy_sdr_output_settings = soapy_sdr_output_settings;
    this->m_soapy_sdr_output_settings_isSet = true;
}

SWGTestSourceSettings*
SWGDeviceSettings::getTestSourceSettings() {
    return test_source_settings;
}
void
SWGDeviceSettings::setTestSourceSettings(SWGTestSourceSettings* test_source_settings) {
    this->test_source_settings = test_source_settings;
    this->m_test_source_settings_isSet = true;
}

SWGXtrxInputSettings*
SWGDeviceSettings::getXtrxInputSettings() {
    return xtrx_input_settings;
}
void
SWGDeviceSettings::setXtrxInputSettings(SWGXtrxInputSettings* xtrx_input_settings) {
    this->xtrx_input_settings = xtrx_input_settings;
    this->m_xtrx_input_settings_isSet = true;
}

SWGXtrxOutputSettings*
SWGDeviceSettings::getXtrxOutputSettings() {
    return xtrx_output_settings;
}
void
SWGDeviceSettings::setXtrxOutputSettings(SWGXtrxOutputSettings* xtrx_output_settings) {
    this->xtrx_output_settings = xtrx_output_settings;
    this->m_xtrx_output_settings_isSet = true;
}


bool
SWGDeviceSettings::isSet(){
    bool isObjectUpdated = false;
    do{
        if(device_hw_type != nullptr && *device_hw_type != QString("")){ isObjectUpdated = true; break;}
        if(m_direction_isSet){ isObjectUpdated = true; break;}
        if(m_originator_index_isSet){ isObjectUpdated = true; break;}
        if(airspy_settings != nullptr && airspy_settings->isSet()){ isObjectUpdated = true; break;}
        if(airspy_hf_settings != nullptr && airspy_hf_settings->isSet()){ isObjectUpdated = true; break;}
        if(blade_rf1_input_settings != nullptr && blade_rf1_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(blade_rf2_input_settings != nullptr && blade_rf2_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(blade_rf1_output_settings != nullptr && blade_rf1_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(blade_rf2_output_settings != nullptr && blade_rf2_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(fcd_pro_settings != nullptr && fcd_pro_settings->isSet()){ isObjectUpdated = true; break;}
        if(fcd_pro_plus_settings != nullptr && fcd_pro_plus_settings->isSet()){ isObjectUpdated = true; break;}
        if(file_source_settings != nullptr && file_source_settings->isSet()){ isObjectUpdated = true; break;}
        if(hack_rf_input_settings != nullptr && hack_rf_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(hack_rf_output_settings != nullptr && hack_rf_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(lime_sdr_input_settings != nullptr && lime_sdr_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(lime_sdr_output_settings != nullptr && lime_sdr_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(local_input_settings != nullptr && local_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(perseus_settings != nullptr && perseus_settings->isSet()){ isObjectUpdated = true; break;}
        if(pluto_sdr_input_settings != nullptr && pluto_sdr_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(pluto_sdr_output_settings != nullptr && pluto_sdr_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(rtl_sdr_settings != nullptr && rtl_sdr_settings->isSet()){ isObjectUpdated = true; break;}
        if(remote_output_settings != nullptr && remote_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(remote_input_settings != nullptr && remote_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(sdr_play_settings != nullptr && sdr_play_settings->isSet()){ isObjectUpdated = true; break;}
        if(soapy_sdr_input_settings != nullptr && soapy_sdr_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(soapy_sdr_output_settings != nullptr && soapy_sdr_output_settings->isSet()){ isObjectUpdated = true; break;}
        if(test_source_settings != nullptr && test_source_settings->isSet()){ isObjectUpdated = true; break;}
        if(xtrx_input_settings != nullptr && xtrx_input_settings->isSet()){ isObjectUpdated = true; break;}
        if(xtrx_output_settings != nullptr && xtrx_output_settings->isSet()){ isObjectUpdated = true; break;}
    }while(false);
    return isObjectUpdated;
}
}

