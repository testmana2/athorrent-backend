#include "AlertManager.h"
#include "ResumeDataManager.h"
#include "Json.h"
#include "Utils.h"

#include <vector>
#include <iostream>
#include <boost/thread.hpp>


using namespace std;

namespace lt = libtorrent;

AlertManager::AlertManager(TorrentManager & torrentManager) : m_torrentManager(torrentManager), m_session(torrentManager.getSession()) {}

void AlertManager::start()
{
    // i use boost threads because c++ threads are destroyed
    // when they go out of scope and i'm using lambda
    // expressions because boost::bind cause extremly stranges
    // behaviors when bind calls are nested
    boost::thread([this] {
        run();
    });
}

void AlertManager::run()
{
    lt::time_duration timeout = lt::seconds(10);

    while (true) {
        m_session.wait_for_alert(timeout);

        vector<lt::alert *> alerts;
        m_session.pop_alerts(&alerts);

        for (auto alert : alerts) {
            handleAlert(alert);
        }
    }
}

void AlertManager::handleAlert(lt::alert * alert)
{
    cout << "alert: " << alert->message() << endl;

    switch (alert->type()) {
        case lt::save_resume_data_alert::alert_type:
            handleSaveResumeDataAlert(lt::alert_cast<lt::save_resume_data_alert>(alert));
            break;

        case lt::save_resume_data_failed_alert::alert_type:
            handleSaveResumeDataFailedAlert(lt::alert_cast<lt::save_resume_data_failed_alert>(alert));
            break;

        case lt::fastresume_rejected_alert::alert_type:
            handleFastresumeRejectedAlert(lt::alert_cast<lt::fastresume_rejected_alert>(alert));
            break;

        case lt::metadata_received_alert::alert_type:
            handleMetadataReceivedAlert(lt::alert_cast<lt::metadata_received_alert>(alert));
            break;

        case lt::torrent_finished_alert::alert_type:
            handleTorrentFinishedAlert(lt::alert_cast<lt::torrent_finished_alert>(alert));
            break;
    }
}

void AlertManager::handleSaveResumeDataAlert(lt::save_resume_data_alert * alert)
{
    m_torrentManager.getResumeDataManager().saveResumeData(alert->handle, alert->resume_data);
}

void AlertManager::handleSaveResumeDataFailedAlert(lt::save_resume_data_failed_alert * alert) {}

void AlertManager::handleFastresumeRejectedAlert(lt::fastresume_rejected_alert * alert) {}

void AlertManager::handleMetadataReceivedAlert(lt::metadata_received_alert * alert)
{
    m_torrentManager.createTorrent(alert->handle);
}

void AlertManager::handleTorrentFinishedAlert(lt::torrent_finished_alert * alert)
{
    lt::torrent_handle & handle = alert->handle;
    
    boost::thread([this, &handle] {
        m_torrentManager.getResumeDataManager().requestSaveResumeData(handle);
    });
    
    if (!m_frontendBinPath.empty()) {
        lt::torrent_status status = handle.status(lt::torrent_handle::query_save_path | lt::torrent_handle::query_name);

        JSON_OBJECT
        JSON_ADD_STRING("hash", lt::to_hex(handle.info_hash().to_string()))
        JSON_ADD_STRING("path", status.save_path + '/' + Utils::fromUtf8(status.name))
        JSON_WRITE(json)

        system((m_frontendBinPath + " triggerHook torrentFinished " + json).c_str());
    }
}

void AlertManager::setFrontendBinPath(const string & frontendBinPath)
{
    m_frontendBinPath = frontendBinPath;
}