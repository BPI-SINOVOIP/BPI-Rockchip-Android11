# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import socket
import threading
import Queue

_BUF_SIZE = 4096

class FakePrinter():
    """
    A fake printer (server).

    It starts a thread that listens on given localhost's port and saves
    incoming documents in the internal queue. Documents can be fetched from
    the queue by calling the fetch_document() method. At the end, the printer
    must be stopped by calling the stop() method. The stop() method is called
    automatically when the object is managed by "with" statement.
    See test_fake_printer.py for examples.

    """

    def __init__(self, port):
        """
        Initialize fake printer.

        It configures the socket and starts the printer. If no exceptions
        are thrown (the method succeeded), the printer must be stopped by
        calling the stop() method.

        @param port: port number on which the printer is supposed to listen

        @raises socket or thread related exception in case of failure

        """
        # If set to True, the printer is stopped either by invoking stop()
        # method or by an internal error
        self._stopped = False
        # It is set when printer is stopped because of some internal error
        self._error_message = None
        # An internal queue with printed documents
        self._documents = Queue.Queue()
        # Create a TCP/IP socket
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            # Bind the socket to the port
            self._socket.bind( ('localhost', port) )
            # Start thread
            self._thread = threading.Thread(target = self._thread_read_docs)
            self._thread.start();
        except:
            # failure - the socket must be closed before exit
            self._socket.close()
            raise


    # These methods allow to use the 'with' statement to automaticaly stop
    # the printer
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, traceback):
        self.stop()


    def stop(self):
        """
        Stops the printer.

        """
        self._stopped = True
        self._thread.join()


    def fetch_document(self, timeout):
        """
        Fetches the next document from the internal queue.

        This method returns the next document and removes it from the internal
        queue. If there is no documents in the queue, it blocks until one
        arrives. If waiting time exceeds a given timeout, an exception is
        raised.

        @param timeout: max waiting time in seconds

        @returns next document from the internal queue

        @raises Exception if the timeout was reached

        """
        try:
            return self._documents.get(block=True, timeout=timeout)
        except Queue.Empty:
            # Builds a message for the exception
            message = 'Timeout occured when waiting for the document. '
            if self._stopped:
                message += 'The fake printer was stopped '
                if self._error_message is None:
                    message += 'by the stop() method.'
                else:
                    message += 'because of the error: %s.' % self._error_message
            else:
                message += 'The fake printer is in valid state.'
            # Raises and exception
            raise Exception(message)


    def _read_whole_document(self):
        """
        Reads a document from the printer's socket.

        It assumes that operation on sockets may timeout.

        @returns whole document or None, if the printer was stopped

        """
        # Accepts incoming connection
        while True:
            try:
                (connection, client_address) = self._socket.accept()
                # success - exit the loop
                break
            except socket.timeout:
                # exit if the printer was stopped, else return to the loop
                if self._stopped:
                    return None

        # Reads document
        document = ''
        while True:
            try:
                data = connection.recv(_BUF_SIZE)
                # success - check data and continue
                if not data:
                    # we got the whole document - exit the loop
                    break
                # save chunk of the document and return to the loop
                document += data
            except socket.timeout:
                # exit if the printer was stopped, else return to the loop
                if self._stopped:
                    connection.close()
                    return None

        # Closes connection & returns document
        connection.close()
        return document


    def _thread_read_docs(self):
        """
        Reads documents from the printer's socket and adds them to the
        internal queue.

        It exits when the printer is stopped by the stop() method.
        In case of any error (exception) it stops the printer and exits.

        """
        try:
            # Listen for incoming printer request.
            self._socket.listen(1)
            # All following socket's methods throw socket.timeout after
            # 500 miliseconds
            self._socket.settimeout(0.5)

            while True:
                # Reads document from the socket
                document = self._read_whole_document()
                # 'None' means that the printer was stopped -> exit
                if document is None:
                    break
                # Adds documents to the internal queue
                self._documents.put(document)
        except BaseException as e:
            # Error occured, the printer must be stopped -> exit
            self._error_message = str(e)
            self._stopped = True

        # Closes socket before the exit
        self._socket.close()
