# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import md5
import os
import requests

# ==================== Documents digests

def _read_lines_with_prefix(document, position, prefix):
    """
    Starting from given position, it parses from the document complete lines
    (with '\n' character at the end) starting from given prefix. Parser stops
    on the first line that does not start from the given prefix or when there
    are no more '\n' characters in the file.

    @param document: a document to parse
    @param position: an offset in the document to start from

    @returns a pair (lines, position), where the first element is a list of
        parsed lines (with '\n' character at the end) and the second element
        is a new offset in the document, pointing at the first character after
        the last parsed line

    """
    lines = []
    while document.startswith(prefix, position):
        position_next_line = document.find('\n', position + len(prefix))
        if position_next_line < 0:
            break
        position_next_line += 1  # to eat '\n' character
        lines.append(document[position:position_next_line])
        position = position_next_line
    return lines, position


def _process_PJL_headers(doc, position, out):
    """
    The function tries to find a PJL headers in given document and process
    them as it was described in _normalize_document(doc) function.

    @param doc: see the description of _normalize_document(doc)
    @param position: offset in the document; defines part of the document that
            is already processed; searching for headers starts from this
            position
    @param out: already processed part of the document (from the beginning to
            the given position)

    @returns new position and output; the position is set at the end of the last
            processed PJL header or it is a copy of of input position, if no PJL
            headers have been foound; the output is adjusted accordingly.

    """
    PJL_MARKER = b'\x1B%-12345X'
    MARGIN = 2048  # max distance to the header
    position_pjl = doc.find(PJL_MARKER, position, position + MARGIN)
    while position_pjl >= 0:
        out += doc[position:(position_pjl+len(PJL_MARKER))]
        position = position_pjl + len(PJL_MARKER)
        # parse header and filter problematic lines
        lines, position = _read_lines_with_prefix(doc, position, '@PJL')
        for line in lines:
            if not (line.startswith('@PJL SET ') or
                    line.startswith('@PJL COMMENT') or
                    line.startswith('@PJL DMINFO') or
                    line.startswith('@PJL JOB NAME') or
                    line.startswith('@PJL JOBNAME')):
                out += line
        # try to find next PJL header
        position_pjl = doc.find(PJL_MARKER, position, position + MARGIN)
    return position, out


def _process_PS_Adobe_headers(doc, position, out):
    """
    The function tries to find a PS-Adobe headers in given document and process
    them as it was described in _normalize_document(doc) function.

    @param doc: see the description of _normalize_document(doc)
    @param position: offset in the document; defines part of the document that
            is already processed; searching for headers starts from this
            position
    @param out: already processed part of the document (from the beginning to
            the given position)

    @returns new position and output; the position is set at the end of the last
            processed PS-Adobe header or it is a copy of of input position, if
            no PS-Adobe headers have been foound; the output is adjusted
            accordingly.

    """
    PS_MARKER = '%!PS-Adobe'
    MARGIN = 2048  # max distance to the header
    position_ps = doc.find(PS_MARKER, position, position + MARGIN)
    while position_ps >= 0:
        # add everything till the end of the first line in the header
        position_next_line = doc.find('\n', position_ps + len(PS_MARKER))
        if position_next_line < 0:
            break  # no more '\n', we finish the parsing here
        position_next_line += 1 # to eat \n character
        out += doc[position:position_next_line]
        # parse the rest of the header and filter problematic lines
        lines, position = _read_lines_with_prefix(doc, position_next_line, '%')
        for line in lines:
            if not (line.startswith('%%Title:') or line.startswith('%%For:')):
                out += line
        # search for lines with '{setuserinfo}' or '/JobInfo <<'
        position_ps = doc.find(PS_MARKER, position, position + MARGIN)
        position_ui = doc.find('{setuserinfo}', position, position + MARGIN)
        position_ji = doc.find('/JobInfo <<', position, position + MARGIN)
        # if '/JobInfo <<' was found, move the offset to the end of the section
        if position_ji >= 0:
            position_ji = doc.find('>>', position_ji)
        # if the beginning of the next header was found, make sure that
        # detected sections do not belong to the next header
        if position_ps >= 0:
            if position_ji > position_ps:
                position_ji = -1
            if position_ui > position_ps:
                position_ui = -1
        # choose the farthest section
        position_end = max(position_ji, position_ui)
        if position_end >= 0:
            # find the first '\n' after the farthest section
            position_end = doc.find('\n', position_end)
            if position_end < 0:
                break  # no more '\n', we finish the parsing here
            # split into lines everything from here to the end of the section
            lines = doc[position:position_end].split('\n')
            position = position_end + 1  # +1 is needed to eat the last \n
            # filter problematic lines
            for line in lines:
                if not (line.find('{setuserinfo}') >= 0 or
                        line.find('/UserID') >= 0 or
                        line.find('/Time') >= 0 or
                        line.find('/HostLoginName') >= 0 or
                        line.find('/HostName') >= 0):
                    out += line + '\n'
            # go to the next iteration, position_ps is already set
    return position, out


def _normalize_LIDIL(doc):
    """
    The function tries to proces given document as it was described in
    _normalize_document(doc) function, but assuming that the document is in
    LIDIL format. This format is used by some HP printers.

    @param doc: see the description of _normalize_document(doc)

    @returns None if the give ndocument is not in LIDIL format. Otherwise, it
        returns a result for _normalize_document(doc) function.

    """
    LIDIL_MARKER = b'\x24\x01\x00\x00\x07\x00\x00\x00'
    LIDIL_JOBID_1_OFF = 2348 # first job id, offset from the beginning
    LIDIL_JOBID_2_OFF = 2339 # second job id, offset from the end
    JOBID_SIZE = 4 # number of bytes used to store job id
    # the document is in LIDIL format <=> it starts with the marker
    if not doc.startswith(LIDIL_MARKER):
        return None
    # remove both JOB IDs and exit
    nd = len(doc)
    if nd > LIDIL_JOBID_1_OFF + LIDIL_JOBID_2_OFF + 2*JOBID_SIZE:
        doc = ''.join([ doc[:(LIDIL_JOBID_1_OFF)],
                doc[(LIDIL_JOBID_1_OFF+JOBID_SIZE):(nd-LIDIL_JOBID_2_OFF)],
                doc[(nd-LIDIL_JOBID_2_OFF+JOBID_SIZE):] ])
    return doc


def _normalize_EJL(doc):
    """
    The function tries to proces given document as it was described in
    _normalize_document(doc) function, but assuming that the document is in
    EJL format.

    @param doc: see the description of _normalize_document(doc)

    @returns None if the give ndocument is not in EJL format. Otherwise, it
        returns a result for _normalize_document(doc) function.

    """
    # EJL - some epson printers (like eplaser)
    EJL_MARKER = b'\x1B\x01@EJL \n'
    # the document is in EJL format <=> it starts with the marker
    if not doc.startswith(EJL_MARKER):
        return None
    # copy the document to output; filter lines parsed from the EJL header
    out = EJL_MARKER
    lines, position = _read_lines_with_prefix(doc, len(EJL_MARKER), '@EJL')
    for line in lines:
        if not (line.startswith('@EJL JI ID=') or
                line.startswith('@EJL JI USER=')):
            out += line
    # add the rest of the document and exit
    out += doc[position:]
    return out


def _normalize_document(doc):
    """
    The input document is a raw package sent to printer. This function removes
    from it all variables that can change, when the same content is printed.
    That includes, but is not limited to: user name, host name, job id, date,
    time.

    @param doc: a raw document sent directly to printer to be printed

    @returns a copy of doc with removed fragments that can vary between
        printing jobs. The returned output is supposed to be identical for the
        same input content send to the pipeline for the same PPD file.

    """
    # Try to parse the document as LIDIL or EJL and exit if successful.
    out = _normalize_LIDIL(doc)
    if out is not None:
        return out
    out = _normalize_EJL(doc)
    if out is not None:
        return out

    # Try to parse and process PJL and PS headers.
    position = 0
    out = ''
    position, out = _process_PJL_headers(doc, position, out)
    position, out = _process_PS_Adobe_headers(doc, position, out)

    # Go to the tail of the document, add the skipped content to the output.
    if position + 2048 < len(doc):
        position_tail = len(doc) - 2048
        out += doc[position:position_tail]
        position = position_tail

    # Try to find 'trailer << '.
    position_trailer = doc.find('trailer << ', position)
    if position_trailer >= 0:
        # If found, prune the line with it.
        position_end = doc.find('\n', position_trailer)
        if position_end >= 0:
            out += doc[position:position_trailer]
            position = position_end + 1  # +1 to ommit '\n' from the trailer

    # Add the rest of the document to the output.
    out += doc[position:]

    return out


def calculate_digest(doc):
    """
    Calculates digests for given document.

    @param doc: document's content

    @returns calculated digests as a string of hexadecimals

    """
    # Prune the variable parts of the document
    out = _normalize_document(doc)

    # Calculates hash
    return md5.new(out).hexdigest()


def parse_digests_file(path_digests, blacklist):
    """
    Parses digests from file.

    @param path_digests: a path to a file with digests
    @param blacklist: list of keys to omit

    @returns a dictionary with digests indexed by ppd filenames or an empty
            dictionary if the given file does not exist

    """
    digests = dict()
    blacklist = set(blacklist)
    if os.path.isfile(path_digests):
        with open(path_digests, 'rb') as file_digests:
            lines = file_digests.read().splitlines()
            for line in lines:
                cols = line.split()
                if len(cols) >= 2 and cols[0] not in blacklist:
                    digests[cols[0]] = cols[1]
    return digests


def save_digests_file(path_digests, digests, blacklist):
    """
    Saves list of digests to file.

    @param digests: dictionary with digests (keys are names)
    @param blacklist: list of keys to ignore

    @return a content of digests file

    """
    digests_content = ''
    names = sorted(set(digests.keys()).difference(blacklist))
    for name in names:
        digest = digests[name]
        assert name.find('\t') < 0 and name.find('\n') < 0
        assert digest.find('\t') < 0 and digest.find('\n') < 0
        digests_content += name + '\t' + digest + '\n'

    with open(path_digests, 'wb') as file_digests:
        file_digests.write(digests_content)


def load_blacklist(path_blacklist):
    """
    Loads blacklist of outputs to omit.

    Raw outputs generated by some PPD files cannot be verified by digests,
    because they contain variables like date/time, job id or other non-static
    parameters. This routine returns list of blacklisted ppds.

    @param path_blacklist: a path to the file with the list of blacklisted
            PPD files

    @returns a list of ppds to ignore during verification of digests

    """
    with open(path_blacklist) as file_blacklist:
        lines = file_blacklist.readlines()

    blacklist = []
    for entry in lines:
        entry = entry.strip()
        if entry != '':
            blacklist.append(entry)

    return blacklist


# ===================== PPD files on the SCS server

def get_filenames_from_PPD_index(task_id):
    """
    It downloads an index file from the SCS server and extracts names
    of PPD files from it.

    @param task_id: an order number of an index file to process; this is
            an integer from the interval [0..20)

    @returns a list of PPD filenames (may contain duplicates)

    """
    # calculates a URL of the index file
    url_metadata = 'https://www.gstatic.com/chromeos_printing/metadata_v2/'
    url_ppd_index = url_metadata + ('index-%02d.json' % task_id)
    # donwloads and parses the index file
    request = requests.get(url_ppd_index)
    entries = json.loads(request.content)
    # extracts PPD filenames (the second element in each index entry)
    output = []
    for entry in entries:
        output.append(entry[1])
    # returns a list of extracted filenames
    return output


def download_PPD_file(ppd_file):
    """
    It downloads a PPD file from the SCS server.

    @param ppd_file: a filename of PPD file (neither path nor URL)

    @returns content of the PPD file
    """
    url_ppds = 'https://www.gstatic.com/chromeos_printing/ppds/'
    request = requests.get(url_ppds + ppd_file)
    return request.content


# ==================== Local filesystem

def list_entries_from_directory(
        path,
        with_suffixes=None, nonempty_results=False,
        include_files=True, include_directories=True ):
    """
    It returns all filenames from given directory. Results may be filtered
    by filenames suffixes or entries types.

    @param path: a path to directory to list files from
    @param with_suffixes: if set, only entries with given suffixes are
            returned; it must be a tuple
    @param nonempty_results: if True then Exception is raised if there is no
            results
    @param include_files: if False, then regular files and links are omitted
    @param include_directories: if False, directories are omitted

    @returns a nonempty list of entries meeting given criteria

    @raises Exception if no matching filenames were found and
            nonempty_results is set to True

    """
    # lists all files from the directory and filter them by given criteria
    list_of_files = []
    for filename in os.listdir(path):
        path_entry = os.path.join(path, filename)
        # check type
        if os.path.isfile(path_entry):
            if not include_files:
                continue
        elif os.path.isdir(path_entry):
            if not include_directories:
                continue
        else:
            continue
        # check suffix
        if with_suffixes is not None:
            if not filename.endswith(with_suffixes):
                continue
        list_of_files.append(filename)
    # throws exception if no files were found
    if nonempty_results and len(list_of_files) == 0:
        message = 'Directory %s does not contain any ' % path
        message += 'entries meeting the criteria'
        raise Exception(message)
    # returns a non-empty list
    return list_of_files
