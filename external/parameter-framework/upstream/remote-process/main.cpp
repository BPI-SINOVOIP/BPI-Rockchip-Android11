/*
 * Copyright (c) 2011-2014, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <asio.hpp>

#include <iostream>
#include <string>
#include <cstring>
#include <stdlib.h>
#include "RequestMessage.h"
#include "AnswerMessage.h"
#include "Socket.h"
#include "convert.hpp"

using namespace std;

bool sendAndDisplayCommand(asio::generic::stream_protocol::socket &socket,
                           CRequestMessage &requestMessage)
{
    string strError;

    if (requestMessage.serialize(Socket(socket), true, strError) != CRequestMessage::success) {

        cerr << "Unable to send command to target: " << strError << endl;
        return false;
    }

    ///// Get answer
    CAnswerMessage answerMessage;
    if (answerMessage.serialize(Socket(socket), false, strError) != CRequestMessage::success) {

        cerr << "Unable to received answer from target: " << strError << endl;
        return false;
    }

    // Success?
    if (!answerMessage.success()) {

        // Display error answer
        cerr << answerMessage.getAnswer() << endl;
        return false;
    }

    // Display success answer
    cout << answerMessage.getAnswer() << endl;

    return true;
}

int usage(const std::string &command, const std::string &error)
{
    if (not error.empty()) {
        cerr << error << endl;
    }
    cerr << "Usage: " << endl;
    cerr << "Send a single command:" << endl;
    cerr << "\t" << command
         << " <hostname port|tcp://[host]:port|unix://path> <command> [argument[s]]" << endl;

    return 1;
}

int main(int argc, char *argv[])
{
    int commandPos;

    // Enough args?
    if (argc < 3) {
        return usage(argv[0], "Missing arguments");
    }
    asio::io_service io_service;
    asio::generic::stream_protocol::socket connectionSocket(io_service);

    bool isInet = false;
    string port;
    string host;
    try {
        // backward compatibility: tcp port only refered by its value
        uint16_t testConverter;
        if (convertTo({argv[2]}, testConverter)) {
            isInet = true;
            port = argv[2];
            host = argv[1];
            if (argc <= 3) {
                return usage(argv[0], "Missing arguments");
            }
            commandPos = 3;
        } else {
            commandPos = 2;
            string endPortArg{argv[1]};
            std::string protocol;

            const std::string tcpProtocol{"tcp"};
            const std::string unixProtocol{"unix"};
            const std::vector<std::string> supportedProtocols{tcpProtocol, unixProtocol};
            const std::string protocolDelimiter{"://"};

            size_t protocolDelPos = endPortArg.find(protocolDelimiter);
            if (protocolDelPos == std::string::npos) {
                return usage(argv[0], "Invalid endpoint " + endPortArg);
            }
            protocol = endPortArg.substr(0, protocolDelPos);

            if (std::find(begin(supportedProtocols), end(supportedProtocols), protocol) ==
                end(supportedProtocols)) {
                return usage(argv[0], "Invalid endpoint " + endPortArg);
            }
            isInet = (endPortArg.find(tcpProtocol) != std::string::npos);
            if (isInet) {
                size_t portDelPos = endPortArg.rfind(':');
                if (portDelPos == std::string::npos) {
                    return usage(argv[0], "Invalid endpoint " + endPortArg);
                }
                host = endPortArg.substr(protocolDelPos + protocolDelimiter.size(),
                                         portDelPos - (protocolDelPos + protocolDelimiter.size()));
                port = endPortArg.substr(portDelPos + 1);
            } else {
                port = endPortArg.substr(protocolDelPos + protocolDelimiter.size());
            }
        }
        if (isInet) {
            asio::ip::tcp::resolver resolver(io_service);
            asio::ip::tcp::socket tcpSocket(io_service);

            asio::connect(tcpSocket, resolver.resolve(asio::ip::tcp::resolver::query(host, port)));
            connectionSocket = std::move(tcpSocket);
        } else {
            asio::generic::stream_protocol::socket socket(io_service);
            asio::generic::stream_protocol::endpoint endpoint =
                asio::local::stream_protocol::endpoint(port);
            socket.connect(endpoint);
            connectionSocket = std::move(socket);
        }

    } catch (const asio::system_error &e) {
        string endpoint;

        if (isInet) {
            endpoint = string("tcp://") + host + ":" + port;
        } else { /* other supported protocols */
            endpoint = argv[1];
        }
        cerr << "Connection to '" << endpoint << "' failed: " << e.what() << endl;
        return 1;
    }

    // Create command message
    CRequestMessage requestMessage(argv[commandPos]);

    // Add arguments
    for (int arg = commandPos + 1; arg < argc; arg++) {

        requestMessage.addArgument(argv[arg]);
    }

    if (!sendAndDisplayCommand(connectionSocket, requestMessage)) {
        return 1;
    }

    // Program status
    return 0;
}
