#include <iostream>
#include <string>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>    
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/errors.hpp>

#include "AthorrentService.h"
#include "TorrentManager.h"
#include "AlertManager.h"
#include "ResumeDataManager.h"

int main(int argc, char * argv[])
{
    boost::program_options::positional_options_description p;
    p.add("user", 1);

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("user", boost::program_options::value<std::string>(), "user id of the user")
        ("frontend-bin", boost::program_options::value<std::string>(), "path to the frontend binary");
    
    boost::program_options::variables_map vm;
    
    try {
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        boost::program_options::notify(vm);
    } catch (boost::program_options::error& except) {
        std::cerr << except.what();
        return EXIT_FAILURE;
    }
    
    if (vm.count("help") || !vm.count("user")) {
        std::cerr << desc << std::endl;
        return EXIT_FAILURE;
    }
    
    std::string userId = vm["user"].as<std::string>();
    
    if (!boost::filesystem::exists("cache")) {
        boost::filesystem::create_directory("cache");
    }
    
    if (!boost::filesystem::exists("files")) {
        boost::filesystem::create_directory("files");
    }
    
    TorrentManager torrentManager(userId);
    
    if (vm.count("frontend-bin")) {
        torrentManager.getAlertManager().setFrontendBinPath(vm["frontend-bin"].as<std::string>());
    }
    
    AthorrentService service(userId, &torrentManager);
    
    boost::asio::io_service ioService;
    torrentManager.getResumeDataManager().start(ioService);
    
    boost::asio::signal_set signals(ioService, SIGINT, SIGTERM);

    signals.async_wait([&service, &torrentManager](const boost::system::error_code & error, int signal_number) {
        if (signal_number == SIGINT) {
            std::cout << "caught SIGINT" << std::endl;
        } else if (signal_number == SIGTERM) {
            std::cout << "caught SIGTERM" << std::endl;
        }
        
        torrentManager.getResumeDataManager().stop();

        service.stop();
        
        exit(EXIT_SUCCESS);
    });

    service.start();
    ioService.run();

	return EXIT_SUCCESS;
}
