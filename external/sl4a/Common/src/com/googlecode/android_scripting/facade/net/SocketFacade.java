/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.googlecode.android_scripting.facade.net;

import android.net.NetworkUtils;
import android.os.ParcelFileDescriptor;
import android.system.ErrnoException;
import android.system.Os;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;

/**
 * This class has APIs for various socket types
 * android.system.Os socket used for UDP and TCP sockets
 * java.net.DatagramSocket for UDP sockets
 * java.net.Socket and java.net.ServerSocket for TCP sockets
 */
public class SocketFacade extends RpcReceiver {

    public static HashMap<String, DatagramSocket> sDatagramSocketHashMap =
            new HashMap<String, DatagramSocket>();
    private static HashMap<String, Socket> sSocketHashMap =
            new HashMap<String, Socket>();
    private static HashMap<String, ServerSocket> sServerSocketHashMap =
            new HashMap<String, ServerSocket>();
    private static HashMap<String, FileDescriptor> sFileDescriptorHashMap =
            new HashMap<String, FileDescriptor>();
    public static int MAX_BUF_SZ = 2048;
    public static int SOCK_TIMEOUT = 1500;

    public SocketFacade(FacadeManager manager) {
        super(manager);
    }

    /**
     * Method to return hash value of a DatagramSocket object
     * Assumes that a unique hashCode is generated for each object
     * and odds of equal hashCode for different sockets is negligible
     */
    private String getDatagramSocketId(DatagramSocket socket) {
        return "DATAGRAMSOCKET:" + socket.hashCode();
    }

    /**
     * Method to return hash value of a Socket object
     * Assumes that a unique hashCode is generated for each object
     * and odds of equal hashCode for different sockets is negligible
     */
    private String getSocketId(Socket socket) {
        return "SOCKET:" + socket.hashCode();
    }

    /**
     * Method to return hash value of a ServerSocket object
     * Assumes that a unique hashCode is generated for each object
     * and odds of equal hashCode for different sockets is negligible
     */
    private String getServerSocketId(ServerSocket socket) {
        return "SERVERSOCKET:" + socket.hashCode();
    }

    /**
     * Method to return hash value of a FileDescriptor object
     * Assumes that a unique hashCode is generated for each object
     * and odds of equal hashCode for different sockets is negligible
     */
    private String getFileDescriptorId(FileDescriptor fd) {
        return "FILEDESCRIPTOR:" + fd.hashCode();
    }

    /**
     * Method to retrieve FileDescriptor from hash key
     * @param id : Hash key of {@link FileDescriptor}
     * @return FileDescriptor
     */
    public static FileDescriptor getFileDescriptor(String id) {
        return sFileDescriptorHashMap.get(id);
    }

    /**
     * Method to retrieve DatagramSocket from hash key
     * @param datagramSocketId : Hash key of {@link DatagramSocket}
     * @return DatagramSocket
     */
    public static DatagramSocket getDatagramSocket(String datagramSocketId) {
        return sDatagramSocketHashMap.get(datagramSocketId);
    }

    /**
     * Method to retrieve Socket from hash key
     * @param socketId : Hash key of {@link Socket}
     * @return Socket
     */
    public static Socket getSocket(String socketId) {
        return sSocketHashMap.get(socketId);
    }

    /**
     * Method to retrieve ServerSocket from hash key
     * @param serverSocketId : Hash key of {@link ServerSocket}
     * @return ServerSocket
     */
    public static ServerSocket getServerSocket(String serverSocketId) {
        return sServerSocketHashMap.get(serverSocketId);
    }

    /*
     * The following APIs for open, close, send and recv over Stream sockets
     * This uses java.net.Socket and java.net.ServerSocket and applies only for TCP traffic
     */

    /**
     * Open TCP client socket and connect to server using java.net.Socket
     * @param remote : IP addr of the server
     * @param remotePort : Port of the server's socket
     * @param local : IP addr of the client
     * @param localPort : Port the client's socket
     * @return Hash key of {@link Socket}
     */
    @Rpc(description = "Open TCP socket & connect to server")
    public String openTcpSocket(
            String remote,
            Integer remotePort,
            String local,
            Integer localPort) {
        try {
            InetAddress remoteAddr = NetworkUtils.numericToInetAddress(remote);
            InetAddress localAddr = NetworkUtils.numericToInetAddress(local);
            Socket socket = new Socket(remoteAddr, remotePort.intValue(), localAddr,
                    localPort.intValue());
            socket.setSoLinger(true, 0);
            String id = getSocketId(socket);
            sSocketHashMap.put(id, socket);
            return id;
        } catch (IOException e) {
            Log.e("Socket: Failed to open TCP client socket ", e);
        }
        return null;
    }

    /**
     * Return port number of Socket
     * @param socketId : Hash key of {@link Socket}
     * @return Port number in int
     */
    @Rpc(description = "Get port number of a Socket")
    public int getPortOfSocket(String socketId) {
        Socket socket = sSocketHashMap.get(socketId);
        return socket.getLocalPort();
    }

    /**
     * Close socket of java.net.Socket class
     * @param socketId : Hash key of {@link Socket}
     * @return True if closing socket is successful
     */
    @Rpc(description = "Close TCP client socket")
    public Boolean closeTcpSocket(String socketId) {
        Socket socket = sSocketHashMap.get(socketId);
        try {
            socket.close();
            sSocketHashMap.remove(socketId);
            return true;
        } catch (IOException e) {
            Log.e("Socket: Failed to close TCP client socket ", e);
        }
        return false;
    }

    /**
     * Open TCP server socket using java.net.ServerSocket
     * @param addr : IP addr of the server
     * @param port : Port of the server's socket
     * @return Hash key of {@link ServerSocket}
     */
    @Rpc(description = "Open TCP server socket and accept connection")
    public String openTcpServerSocket(String addr, Integer port) {
        try {
            InetAddress localAddr = NetworkUtils.numericToInetAddress(addr);
            ServerSocket serverSocket = new ServerSocket(port.intValue(), 10, localAddr);
            String id = getServerSocketId(serverSocket);
            sServerSocketHashMap.put(id, serverSocket);
            return id;
        } catch (IOException e) {
            Log.e("Socket: Failed to open TCP server socket ", e);
        }
        return null;
    }

    /**
     * Return port number of ServerSocket
     * @param serverSocketId : Hash key of {@link ServerSocket}
     * @return Port number in int
     */
    @Rpc(description = "Get port number of a ServerSocket")
    public int getPortOfServerSocket(String serverSocketId) {
        ServerSocket socket = sServerSocketHashMap.get(serverSocketId);
        return socket.getLocalPort();
    }

    /**
     * Close TCP server socket
     * @param serverSocketId : Hash key of {@link ServerSocket}
     * @return True if server socket is closed
     */
    @Rpc(description = "Close TCP server socket")
    public Boolean closeTcpServerSocket(String serverSocketId) {
        ServerSocket socket = sServerSocketHashMap.get(serverSocketId);
        try {
            socket.close();
            sServerSocketHashMap.remove(serverSocketId);
            return true;
        } catch (IOException e) {
            Log.e("Socket: Failed to close TCP server socket ", e);
        }
        return false;
    }

    /**
     * Get FileDescriptor of a Socket object
     * @param socketId : Hash key of {@link Socket}
     * @return Filedescriptor object in string
     */
    @Rpc(description = "Get FileDescriptor object of socket")
    public String getFileDescriptorOfSocket(String socketId) {
        Socket socket = sSocketHashMap.get(socketId);
        ParcelFileDescriptor pfd = ParcelFileDescriptor.fromSocket(socket);
        FileDescriptor fd = pfd.getFileDescriptor();
        String fdId = getFileDescriptorId(fd);
        sFileDescriptorHashMap.put(fdId, fd);
        return fdId;
    }

    /**
     * Shutdown a FileDescriptor
     * @param id : Hash key of {@link FileDescriptor}
     * @return true if shutdown of Filedescriptor is successful, false if not
     */
    @Rpc(description = "Shutdown FileDescriptor")
    public Boolean shutdownFileDescriptor(String id) {
        try {
            FileDescriptor fd = sFileDescriptorHashMap.get(id);
            Os.shutdown(fd, 2);
            return true;
        } catch (ErrnoException e) {
            Log.e("Socket: Failed to shutdown FileDescriptor ", e);
        }
        return false;
    }

    /**
     * Get the local port on which the ServerSocket is listening. Useful when the server socket
     * is initialized with 0 port (i.e. selects an available port)
     *
     * @param serverSocketId : Hash key of ServerSocket (returned by
     * {@link #openTcpServerSocket(String, Integer)}.
     * @return An integer - the port number (0 in case of an error).
     */
    @Rpc(description = "Get the TCP Server socket port number")
    public Integer getTcpServerSocketPort(String serverSocketId) {
        ServerSocket socket = sServerSocketHashMap.get(serverSocketId);
        return socket.getLocalPort();
    }

    /**
     * Accept TCP connection
     * @param serverSocketId : Hash key of ServerSocket
     * @return Hash key of {@link Socket}
     */
    @Rpc(description = "Accept connection")
    public String acceptTcpSocket(String serverSocketId) {
        try {
            ServerSocket serverSocket = sServerSocketHashMap.get(serverSocketId);
            Socket socket = serverSocket.accept();
            String sockId = getSocketId(socket);
            sSocketHashMap.put(sockId, socket);
            return sockId;
        } catch (IOException e) {
            Log.e("Socket: Failed to accept connection ", e);
        }
        return null;
    }

    /**
     * Send data to server - only ASCII/UTF characters
     * @param socketId : Hash key of {@link Socket}
     * @param message : Data to send to in String
     * @return True if sending data is successful, false if not
     */
    @Rpc(description = "Send data from client")
    public Boolean sendDataOverTcpSocket(String socketId, String message) {
        Socket socket = sSocketHashMap.get(socketId);
        try {
            ObjectOutputStream oos = new ObjectOutputStream(socket.getOutputStream());
            oos.writeObject(message);
            return true;
        } catch (IOException | SecurityException | IllegalArgumentException e) {
            Log.e("Socket: Failed to send data from socket ", e);
        }
        return false;
    }

    /**
     * Receive data on ServerSocket - only ASCII/UTF characters
     * @param serverSocketId : Hash key of {@link ServerSocket}
     * @return Received data in String, null if unabled to read from socket
     */
    @Rpc(description = "Recv data from client")
    public String recvDataOverTcpSocket(String serverSocketId) {
        Socket socket = sSocketHashMap.get(serverSocketId);
        try {
            ObjectInputStream ois = new ObjectInputStream(socket.getInputStream());
            String message = (String) ois.readObject();
            Log.d("Socket: Received " + message);
            return message;
        } catch (IOException | SecurityException | IllegalArgumentException
                | ClassNotFoundException e) {
            Log.e("Socket: Failed to read from socket ", e);
        }
        return null;
    }

    /**
     * Send data over TCP socket using DataOutputStream
     * @param socketId : Hash key of {@link Socket}
     * @param message : Data to send in string
     * @return true if sending data is successful, false if not
     */
    @Rpc(description = "Send data from client using DataOutputStream")
    public Boolean tcpSocketWrite(String socketId, String message) {
        Socket socket = sSocketHashMap.get(socketId);
        try {
            DataOutputStream out = new DataOutputStream(socket.getOutputStream());
            Log.d("Socket: Sending -> " + message);
            out.write(message.getBytes(), 0, message.length());
            out.flush();
            return true;
        } catch (IOException | SecurityException | IllegalArgumentException e) {
            Log.e("Socket: Failed to read from socket ", e);
        }
        return false;
    }

    /**
     * Receive data over TCP socket using DataInputStream
     * @param socketId : Hash key of {@link Socket}
     * @return String message if received data, null if not
     */
    @Rpc(description = "Recv data from server using DataInputStream")
    public String tcpSocketRecv(String socketId) {
        Socket socket = sSocketHashMap.get(socketId);
        try {
            byte[] messageByte = new byte[1024];
            DataInputStream in = new DataInputStream(socket.getInputStream());
            int bytesRead = in.read(messageByte);
            String message = new String(messageByte, 0, bytesRead);
            Log.d("Socket: Received -> " + message);
            return message;
        } catch (IOException | SecurityException | IllegalArgumentException e) {
            Log.e("Socket: Failed to read from socket ", e);
        }
        return null;
    }

    /*
     * The following APIs are used to open, close, send and recv data over DatagramSocket
     * This uses java.net.DatagramSocket class and only applies for UDP
     */

    /**
     * Open datagram socket using java.net.DatagramSocket
     * @param addr : IP addr to use
     * @param port : port to open socket on
     * @return Hash key of {@link DatagramSocket}
     */
    @Rpc(description = "Open datagram socket")
    public String openDatagramSocket(String addr, Integer port) {
        InetAddress localAddr = NetworkUtils.numericToInetAddress(addr);
        try {
            DatagramSocket socket = new DatagramSocket(port.intValue(), localAddr);
            socket.setSoTimeout(SOCK_TIMEOUT);
            String id = getDatagramSocketId(socket);
            sDatagramSocketHashMap.put(id, socket);
            return id;
        } catch (SocketException e) {
            Log.e("Socket: Failed to open datagram socket");
        }
        return null;
    }

    /**
     * Return port number of DatagramSocket
     * @param datagramSocketId : Hash key of {@link DatagramSocket}
     * @return Port number in int
     */
    @Rpc(description = "Get port number of a DatagramSocket")
    public int getPortOfDatagramSocket(String datagramSocketId) {
        DatagramSocket socket = sDatagramSocketHashMap.get(datagramSocketId);
        return socket.getLocalPort();
    }

    /**
     * Close datagram socket
     * @param datagramSocketId : Hash key of {@link DatagramSocket}
     * @return True if close is successful
     */
    @Rpc(description = "Close datagram socket")
    public Boolean closeDatagramSocket(String datagramSocketId) {
        DatagramSocket socket = sDatagramSocketHashMap.get(datagramSocketId);
        socket.close();
        sDatagramSocketHashMap.remove(datagramSocketId);
        return true;
    }

    /**
     * Send data from datagram socket to server.
     * @param datagramSocketId : Hash key of {@link DatagramSocket}
     * @param message : data to send in String
     * @param addr : IP addr to send the data to
     * @param port : port of the socket to send the data to
     * @return True if sending data is successful
     */
    @Rpc(description = "Send data over socket", returns = "True if sending data successful")
    public Boolean sendDataOverDatagramSocket(String datagramSocketId, String message, String addr,
            Integer port) {
        byte[] buf = message.getBytes();
        try {
            InetAddress remoteAddr = NetworkUtils.numericToInetAddress(addr);
            DatagramSocket socket = sDatagramSocketHashMap.get(datagramSocketId);
            DatagramPacket pkt = new DatagramPacket(buf, buf.length, remoteAddr, port.intValue());
            socket.send(pkt);
            return true;
        } catch (IOException e) {
            Log.e("Socket: Failed to send data over datagram socket");
        }
        return false;
    }

    /**
     * Receive data on the datagram socket
     * @param datagramSocketId : Hash key of {@link DatagramSocket}
     * @return Received data in String format
     */
    @Rpc(description = "Receive data over socket", returns = "Received data in String")
    public String recvDataOverDatagramSocket(String datagramSocketId) {
        byte[] buf = new byte[MAX_BUF_SZ];
        try {
            DatagramSocket socket = sDatagramSocketHashMap.get(datagramSocketId);
            DatagramPacket dgramPacket = new DatagramPacket(buf, MAX_BUF_SZ);
            socket.receive(dgramPacket);
            return new String(dgramPacket.getData(), 0, dgramPacket.getLength());
        } catch (IOException e) {
            Log.e("Socket: Failed to recv data over datagram socket");
        }
        return null;
    }

    /*
     * The following APIs are used to open, close, send and receive data over Os.socket
     * This uses android.system.Os class and can be used for UDP and TCP traffic
     */

    /**
     * Open socket using android.system.Os class.
     * @param domain : protocol family. Ex: IPv4 or IPv6
     * @param type : socket type. Ex: DGRAM or STREAM
     * @param addr : IP addr to use
     * @param port : port to open socket on
     * @return Hash key of {@link FileDescriptor}
     */
    @Rpc(description = "Open socket")
    public String openSocket(Integer domain, Integer type, String addr, Integer port) {
        try {
            FileDescriptor fd = Os.socket(domain, type, 0);
            InetAddress localAddr = NetworkUtils.numericToInetAddress(addr);
            Os.bind(fd, localAddr, port.intValue());
            String id = getFileDescriptorId(fd);
            sFileDescriptorHashMap.put(id, fd);
            return id;
        } catch (SocketException | ErrnoException e) {
            Log.e("Socket: Failed to open socket ", e);
        }
        return null;
    }

    /**
     * Close socket of android.system.Os class
     * @param id : Hash key of {@link FileDescriptor}
     * @return True if connect successful
     */
    @Rpc(description = "Close socket")
    public Boolean closeSocket(String id) {
        FileDescriptor fd = sFileDescriptorHashMap.get(id);
        try {
            Os.close(fd);
            return true;
        } catch (ErrnoException e) {
            Log.e("Socket: Failed to close socket ", e);
        }
        return false;
    }

    /**
     * Send data from the socket
     * @param remoteAddr : IP addr to send the data to
     * @param remotePort : Port of the socket to send the data to
     * @param message : data to send in String
     * @param id : Hash key of {@link FileDescriptor}
     * @return True if connect successful
     */
    @Rpc(description = "Send data to server")
    public Boolean sendDataOverSocket(
            String remoteAddr, Integer remotePort, String message, String id) {
        FileDescriptor fd = sFileDescriptorHashMap.get(id);
        InetAddress remote = NetworkUtils.numericToInetAddress(remoteAddr);
        try {
            byte [] data = new String(message).getBytes(StandardCharsets.UTF_8);
            int bytes = Os.sendto(fd, data, 0, data.length, 0, remote, remotePort.intValue());
            Log.d("Socket: Sent " + String.valueOf(bytes) + " bytes");
            return true;
        } catch (ErrnoException | SocketException e) {
            Log.e("Socket: Sending data over socket failed ", e);
        }
        return false;
    }

    /**
     * Receive data on the socket.
     * @param id : Hash key of {@link FileDescriptor}
     * @return Received data in String format
     */
    @Rpc(description = "Recv data on server")
    public String recvDataOverSocket(String id) {
        byte[] data = new byte[MAX_BUF_SZ];
        FileDescriptor fd = sFileDescriptorHashMap.get(id);
        try {
            Os.read(fd, data, 0, data.length);
            return new String(data, StandardCharsets.UTF_8);
        } catch (ErrnoException | InterruptedIOException e) {
            Log.e("Socket: Receiving data over socket failed ", e);
        }
        return null;
    }

    /**
     * Listen for connections from client.
     * @param id : Hash key of {@link FileDescriptor}
     * @return True if listen successful
     */
    @Rpc(description = "Listen for connection on server")
    public Boolean listenSocket(String id) {
        FileDescriptor fd = sFileDescriptorHashMap.get(id);
        try {
            // Start listening with buffer of size 10
            Os.listen(fd, 10);
            return true;
        } catch (ErrnoException e) {
            Log.e("Socket: Failed to listen on socket ", e);
        }
        return false;
    }

    /**
     * TCP connect to server from client.
     * @param id : Hash key of {@link FileDescriptor}
     * @param addr : IP addr in string of server
     * @param port : server's port to connect to
     * @return True if connect successful
     */
    @Rpc(description = "Connect to server")
    public Boolean connectSocket(String id, String addr, Integer port) {
        FileDescriptor fd = sFileDescriptorHashMap.get(id);
        try {
            InetAddress remoteAddr = NetworkUtils.numericToInetAddress(addr);
            Os.connect(fd, remoteAddr, port.intValue());
            return true;
        } catch (SocketException | ErrnoException e) {
            Log.e("Socket: Failed to connect socket ", e);
        }
        return false;
    }

    /**
     * Accept TCP connection from the client to server.
     * @param id : Hash key of server {@link FileDescriptor}
     * @return Hash key of FileDescriptor returned by successful accept()
     */
    @Rpc(description = "Accept connection on server")
    public String acceptSocket(String id) {
        FileDescriptor fd = sFileDescriptorHashMap.get(id);
        try {
            FileDescriptor socket = Os.accept(fd, null);
            String socketId = getFileDescriptorId(socket);
            sFileDescriptorHashMap.put(socketId, socket);
            return socketId;
        } catch (SocketException | ErrnoException e) {
            Log.e("Socket: Failed to accept on socket ", e);
        }
        return null;
    }

    @Override
    public void shutdown() {}
}
