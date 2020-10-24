#include <imgui.h>
#include <module.h>
#include <watcher.h>
#include <wav.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <thread>
#include <ctime>
#include <signal_path/audio.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <config.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

// TODO: Fix this and finish module

std::string genFileName(std::string prefix) {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buf[1024];
    sprintf(buf, "%02d-%02d-%02d_%02d-%02d-%02d.wav", ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900);
    return prefix + buf;
}

void streamRemovedHandler(void* ctx) {

}

void sampleRateChanged(void* ctx, double sampleRate, int blockSize) {

}

class RecorderModule {
public:
    RecorderModule(std::string name) {
        this->name = name;
        recording = false;
        selectedStreamName = "";
        selectedStreamId = 0;
        lastNameList = "";
        gui::menu.registerEntry(name, menuHandler, this);
    }

    ~RecorderModule() {

    }

private:
    static void menuHandler(void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

        std::vector<std::string> streamNames = audio::getStreamNameList();
        std::string nameList;
        for (std::string name : streamNames) {
            nameList += name;
            nameList += '\0';
        }

        if (nameList == "") {
            ImGui::Text("No audio stream available");
            return;
        }

        if (_this->lastNameList != nameList) {
            _this->lastNameList = nameList;
            auto _nameIt = std::find(streamNames.begin(), streamNames.end(), _this->selectedStreamName);
            if (_nameIt == streamNames.end()) {
                // TODO: verify if there even is a stream
                _this->selectedStreamId = 0;
                _this->selectedStreamName = streamNames[_this->selectedStreamId];
            }
            else {
                _this->selectedStreamId = std::distance(streamNames.begin(), _nameIt);
                _this->selectedStreamName = streamNames[_this->selectedStreamId];
            }
        }

        ImGui::BeginGroup();

        // TODO: Change VFO ref in signal path

        ImGui::Columns(3, CONCAT("RecordModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("Baseband##_", _this->name), _this->recMode == 0) && _this->recMode != 0) { 
            _this->recMode = 0;
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("Audio##_", _this->name), _this->recMode == 1) && _this->recMode != 1) {
            _this->recMode = 1;
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("VFO##_", _this->name), _this->recMode == 2) && _this->recMode != 2) {
            _this->recMode = 2;
        }
        ImGui::Columns(1, CONCAT("EndRecordModeColumns##_", _this->name), false);

        ImGui::EndGroup();

        if (_this->recMode == 0) {
            ImGui::PushItemWidth(menuColumnWidth);
            if (!_this->recording) {
                if (ImGui::Button("Record", ImVec2(menuColumnWidth, 0))) {
                    _this->samplesWritten = 0;
                    _this->sampleRate = sigpath::signalPath.getSampleRate();
                    _this->writer = new WavWriter(ROOT_DIR "/recordings/" + genFileName("baseband_"), 16, 2, _this->sampleRate);
                    _this->iqStream = new dsp::stream<dsp::complex_t>();
                    _this->iqStream->init(_this->sampleRate / 200.0);
                    sigpath::signalPath.bindIQStream(_this->iqStream);
                    _this->workerThread = std::thread(_iqWriteWorker, _this);
                    _this->recording = true;
                    _this->startTime = time(0);
                }
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
            }
            else {
                if (ImGui::Button("Stop", ImVec2(menuColumnWidth, 0))) {
                    _this->iqStream->stopReader();
                    _this->workerThread.join();
                    _this->iqStream->clearReadStop();
                    sigpath::signalPath.unbindIQStream(_this->iqStream);
                    _this->writer->close();
                    delete _this->writer;
                    _this->recording = false;
                }
                uint64_t seconds = _this->samplesWritten / (uint64_t)_this->sampleRate;
                time_t diff = seconds;
                tm *dtm = gmtime(&diff);
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
            }
        }
        else if (_this->recMode == 1) {
            ImGui::PushItemWidth(menuColumnWidth);
            if (!_this->recording) {
                if (ImGui::Combo(CONCAT("##_strea_select_", _this->name), &_this->selectedStreamId, nameList.c_str())) {
                    _this->selectedStreamName = streamNames[_this->selectedStreamId];
                }
            }
            else {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f, 0.44f, 0.44f, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.21f, 0.22f, 0.30f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 0.65f));
                ImGui::Combo(CONCAT("##_strea_select_", _this->name), &_this->selectedStreamId, nameList.c_str());
                ImGui::PopItemFlag();
                ImGui::PopStyleColor(3);
            }
            if (!_this->recording) {
                if (ImGui::Button("Record", ImVec2(menuColumnWidth, 0))) {
                    _this->samplesWritten = 0;
                    _this->sampleRate = 48000;
                    _this->writer = new WavWriter(ROOT_DIR "/recordings/" + genFileName("audio_"), 16, 2, 48000);
                    _this->audioStream = audio::bindToStreamStereo(_this->selectedStreamName, streamRemovedHandler, sampleRateChanged, _this);
                    _this->workerThread = std::thread(_audioWriteWorker, _this);
                    _this->recording = true;
                    _this->startTime = time(0);
                }
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
            }
            else {
                if (ImGui::Button("Stop", ImVec2(menuColumnWidth, 0))) {
                    _this->audioStream->stopReader();
                    _this->workerThread.join();
                    _this->audioStream->clearReadStop();
                    audio::unbindFromStreamStereo(_this->selectedStreamName, _this->audioStream);
                    _this->writer->close();
                    delete _this->writer;
                    _this->recording = false;
                }
                uint64_t seconds = _this->samplesWritten / (uint64_t)_this->sampleRate;
                time_t diff = seconds;
                tm *dtm = gmtime(&diff);
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
            }
        }
    }

    static void _audioWriteWorker(RecorderModule* _this) {
        dsp::StereoFloat_t* floatBuf = new dsp::StereoFloat_t[1024];
        int16_t* sampleBuf = new int16_t[2048];
        while (true) {
            if (_this->audioStream->read(floatBuf, 1024) < 0) {
                break;
            }
            for (int i = 0; i < 1024; i++) {
                sampleBuf[(i * 2) + 0] = floatBuf[i].l * 0x7FFF;
                sampleBuf[(i * 2) + 1] = floatBuf[i].r * 0x7FFF;
            }
            _this->samplesWritten += 1024;
            _this->writer->writeSamples(sampleBuf, 2048 * sizeof(int16_t));
        }
        delete[] floatBuf;
        delete[] sampleBuf;
    }

    static void _iqWriteWorker(RecorderModule* _this) {
        dsp::complex_t* iqBuf = new dsp::complex_t[1024];
        int16_t* sampleBuf = new int16_t[2048];
        while (true) {
            if (_this->iqStream->read(iqBuf, 1024) < 0) {
                break;
            }
            for (int i = 0; i < 1024; i++) {
                sampleBuf[(i * 2) + 0] = iqBuf[i].q * 0x7FFF;
                sampleBuf[(i * 2) + 1] = iqBuf[i].i * 0x7FFF;
            }
            _this->samplesWritten += 1024;
            _this->writer->writeSamples(sampleBuf, 2048 * sizeof(int16_t));
        }
        delete[] iqBuf;
        delete[] sampleBuf;
    }

    std::string name;
    dsp::stream<dsp::StereoFloat_t>* audioStream;
    dsp::stream<dsp::complex_t>* iqStream;
    WavWriter* writer;
    std::thread workerThread;
    bool recording;
    time_t startTime;
    std::string lastNameList;
    std::string selectedStreamName;
    int selectedStreamId;
    uint64_t samplesWritten;
    float sampleRate;
    int recMode = 0;

};

struct RecorderContext_t {
    std::string name;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    RecorderModule* instance = new RecorderModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_() {
    
}

MOD_EXPORT void _STOP_(RecorderContext_t* ctx) {
    
}