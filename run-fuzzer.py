#!/usr/bin/python

from __future__ import print_function

import logging
logging.basicConfig(level=logging.ERROR)
LOG = logging.getLogger("fuzzer")

import argparse
import commands
import optparse
import os
import platform
import sys
import lldb

import subprocess
from enum import Enum
import time
import sys
import os
from StringIO import StringIO
import signal
import shutil
import re
import random
import string

from datetime import datetime

# Constants

REPORT_DIR = './bugreports'
FIRMSMITH_BIN = os.path.abspath('./build/debug/firmsmith')
CPARSER_BIN = os.path.abspath('./../cparser/build/debug/cparser')

# Global vars

fuzzer_options = None
optimizations = []
now = None
current_report = None

# Params

show_debug = True

def print_debug(*args, **kwargs):
    if show_debug:
        print(*args, file=sys.stderr, **kwargs)

# Models

class DebugPoint:

    def __init__(self):
        self.runtime = -1
        self.stacktrace_frames = []
        self.stacktrace_listing = []
        self.ir_filename = None
        self.vcg_filename = None

    def __str__(self):
        result = ""

        # Append files
        if self.ir_filename != None or self.vcg_filename != None:
            result += "IR Graph was snapshotted at debugging stop:\n"
            if self.ir_filename != None:
                result += "* %s\n" % self.ir_filename
            if self.vcg_filename != None:
                result += "* %s\n" % self.vcg_filename

        # Append stacktrace
        stacktrace_str = '\n'.join(map(lambda x: '\t' + x, self.stacktrace_frames))
        result +=  "Stacktrace after running for %.2f seconds\n\n" % self.runtime
        result += stacktrace_str;
        result += "\n\n"

        return result

class DebugRecord:

    def __init__(self):
        self.args = []
        self.returncode = None
        self.timeout = None
        self.debug_points = []
        self.stderrdata = None

    def __str__(self):
        result = ""

        if self.returncode != None:
            result += "#### cparser aborted with exit code %d\n\n" % self.returncode
            if self.stderrdata:
                LOG.info(self.stderrdata)
                result += "cparser produced the following data on stderr\n\n"
                result += "\t%s\n\n" % self.stderrdata.replace('\n', '\n\t')
        elif self.timeout:
            result += "#### cparser timed out\n\n"
            stacktraces = map(lambda x: x.stacktrace_frames, self.debug_points)
            common_stacktrace = get_common_stacktrace(stacktraces)
            if len(common_stacktrace) > 0:
                result += "Stack trace identicial up to frame:\n\n\t%s\n\n" % common_stacktrace[-1]
        else:
            result == "#### cparser ran successfully\n\n"

        result += "cparser was run with the following options:\n\n"
        result += "\tcparser %s\n\n" % ' '.join(self.args)
        result += '\n'.join(map(str, self.debug_points))

        return result


class Report:
    def __init__(self):
        # Assign id
        Report.index += 1
        self.index = Report.index
        self.strid = get_date_string() + '-' + str(self.index)

        # Debug record lists
        self.records = []
        self.successes = []
        self.aborts = []
        self.timeouts = []
        self.crashes = []

    def is_bug_report(self):
        return len(self.timeouts) > 0 or len(self.aborts) > 0


    def get_identifier(self):
        id_list = []
        re_assertion = re.compile(
            'Assertion failed: .* file (.*), line (\d*)',
            re.MULTILINE
        )

        for record in self.timeouts:
            id_list.append("t_" + '_'.join(record.args[3:]))

        for record in self.aborts:
            record_id = "a_" # "a_" + '_'.join(record.args[3:])
            match = re_assertion.search(record.stderrdata)
            if match:
                record_id += '_'.join(match.groups()).replace('/', '_')
            else:
                 record_id +=  '_'.join(record.args[3:])
            id_list.append(record_id)

        return '_'.join(list(set(id_list[:3])))


    def __str__(self):
        result = '# Report %s\n\n' % self.strid

        result += "The ir graph was generated by running\n\n"
        result += "\tfirmsmith %s\n\n" % self.args

        result += "Cparser version:\n\n\t%s\n\n" % \
            get_cparser_version().replace('\n', '\n\t').strip()

        result += '## Error report\n\n'

        if len(self.timeouts) > 0:
            result += 'The following cparser runs timed out:\n\n'
            for record in self.timeouts:
                result += "\tcparser %s\n" % ' '.join(record.args)
            result += "\n"

        if len(self.aborts) > 0:
            result += 'The following cparser runs aborted\n\n'
            for record in self.aborts:
                result += "\tcparser %s\n" % ' '.join(record.args)
            result += "\n"

        if len(self.crashes) > 0:
            result += 'The following cparser runs crashed:\n\n'
            for record in self.crashes:
                result += "\tcparser %s\n" % ' '.join(record.args)
            result += "\n"

        if len(self.successes) > 0:
            result += 'The following cparser runs succeeded:\n\n'
            for record in self.successes:
                result += "\tcparser %s\n" % ' '.join(record.args)
            result += "\n"

        if len(self.timeouts) > 0:
            result += '### cparser timeouts\n\n'
            for record in self.timeouts:
                result += str(record)

        if len(self.aborts) > 0:
            result += '### cparser aborts\n\n'
            for record in self.aborts:
                result += str(record)

        if len(self.crashes) > 0:
            result += '### cparser crashes:\n\n'
            for record in self.crashes:
                result += str(record)

        return result

Report.index = 1

# Exceptions

class CalledProcessError(Exception):

    def __init__(self, returncode, stderrdata = ""):
        super(CalledProcessError, self).__init__()
        self.returncode = returncode
        self.stderrdata = stderrdata


class TimeoutError(Exception):
    pass

# Random helper function

def get_date_string():
    global now
    return now.strftime('%Y%m%d%H%M%S')


def set_timeout(timeout, func):
    """
    Call provided function and interrupt after given timeout.
    """
    def timeout_handler(*args):
        raise TimeoutError()

    try:
        signal.signal(signal.SIGALRM, timeout_handler)
        signal.alarm(timeout)
        return func()
    finally:
        signal.signal(signal.SIGALRM, lambda *args: None)
    return False

# LLDB helper functions

def get_debugger():
    debugger = lldb.SBDebugger.Create()
    debugger.SetAsync(True)
    return debugger


def get_debugger_target(debugger):
    return debugger.CreateTarget(CPARSER_BIN)


def get_args_as_string(frame, showFuncName=True):
    """
    Returns the args of the input frame object as a string.
    """
    # arguments     => True
    # locals        => False
    # statics       => False
    # in_scope_only => True
    vars = frame.GetVariables(True, False, False, True)  # type of SBValueList
    args = []  # list of strings
    for var in vars:
        node_str = ''
        type = var.GetType()
        if type.IsPointerType():
            type = type.GetPointeeType()
        type_name = type.GetName()
        if type_name.strip().endswith('ir_node'):
            node_nr = var.GetChildMemberWithName('node_nr')
            node_type = var.GetChildMemberWithName('op').GetChildMemberWithName('name')
            node_str = ' [node_nr=%s, node_opname=%s]' % \
               (str(node_nr.GetValue()), str(node_type.GetSummary()))


        args.append("(%s)%s=%s%s" % (var.GetTypeName(),
                                   var.GetName(),
                                   var.GetValue(),
                                   node_str))

    if frame.GetFunction():
        name = frame.GetFunction().GetName()
    elif frame.GetSymbol():
        name = frame.GetSymbol().GetName()
    else:
        name = ""
    if showFuncName:
        return "%s(%s)" % (name, ", ".join(args))
    else:
        return "(%s)" % (", ".join(args))


def get_stacktrace_frames(thread):
    """
    Returns the call stack as list of strings containing the function name and arguments.
    Arguments of type ir_node are annotated with the node number and node type.
    """
    frames = []

    target = thread.GetProcess().GetTarget()
    depth = thread.GetNumFrames()

    i = -1
    for frame in thread.frames:
        i += 1
        #frame = thread.GetFrameAtIndex(i)
        function = frame.GetFunction()

        func   = frame.GetFunctionName() if function else ''
        symbol = frame.GetSymbol().GetName() if not function else ''
        file   = frame.GetLineEntry().GetFileSpec().GetFilename() if frame.GetLineEntry().GetFileSpec() else ''
        line   = frame.GetLineEntry().GetLine()
        addr   = frame.GetPCAddress()

        load_addr = addr.GetLoadAddress(target)
        if not function:
            file_addr = addr.GetFileAddress()
            start_addr = frame.GetSymbol().GetStartAddress().GetFileAddress()
            symbol_offset = file_addr - start_addr
            frames.append(
                "frame #{num}: {addr:#016x} `{symbol} + {offset}".format(
                    num=i,
                    addr=load_addr,
                    symbol=symbol,
                    offset=symbol_offset)
            )
        else:
            frames.append(
                "frame #{num}: {addr:#016x} `{func} at {file}:{line} {args}".format(
                    num=i,
                    addr=load_addr,
                    func='%s [inlined]' %
                         func if frame.IsInlined() else func,
                    file=file,
                    line=line,
                    args=get_args_as_string(
                        frame,
                        showFuncName=False) if not frame.IsInlined() else '()'),
            )

    return frames


def yield_process_events(debugger, process, n_stops = 0):
    """
    Generator to yield events created by debugger for the provided process.
    The process will be interrupted if a time threshold is surpassed and
    it will be killed at latest after n_stops.
    """
    # sign up for process state change events
    stop_idx = 0
    done = False
    listener = debugger.GetListener()

    data = []
    timeout = False
    while not done:
        event = lldb.SBEvent()
        if listener.WaitForEvent(8 if stop_idx == 0 else 1, event):
            event_process = lldb.SBProcess.GetProcessFromEvent(event)
            if event_process and event_process.GetProcessID() == process.GetProcessID():
                state = lldb.SBProcess.GetStateFromEvent(event)
                if state == lldb.eStateInvalid:
                   yield (state, event)
                elif state == lldb.eStateStopped:
                    yield (state, event)
                    stop_idx += 1
                    if not timeout or stop_idx >= n_stops:
                        done = True
                    else:
                        timeout = False
                        process.Continue()
                elif state == lldb.eStateExited:
                    yield (state, event)
                    done = True
                elif state == lldb.eStateCrashed:
                    yield (state, event)
                    done = True
                elif state == lldb.eStateLaunching:
                    pass
                else:
                    LOG.info("lldb event state = "+str(state))
        else:
            LOG.debug("timeout!")
            timeout = True
            process.Stop()
            break

    process.Kill()

# Firmsmith

def get_firmsmith_random_args():
    """
    Create random arguments for firmsmith
    """
    random_bytes = os.urandom(10)
    # Calculate random seed
    seed = 0
    for c in random_bytes[:8]:
        seed = (seed * 256) + ord(c)
    # Calculate random graph size
    graphsize = ord(os.urandom(1)) % 50
    # Calculate random block size
    blocksize = ord(os.urandom(1)) % (50 - graphsize / 2)
    # Return arguments
    return {
        "cfg-size": graphsize,
        "cfb-size": blocksize,
        "seed":     seed
    }


def get_firmsmith_args_as_string(args):
    s = ""
    for k, v in args.iteritems():
        if v == None:
            s += '-' + v
        else:
            s += ' --%s %s' % (k, v)
    return s


def firmsmith_generate_ir_graph(args):
    bin = FIRMSMITH_BIN
    args = [bin] + args.split()
    devnull = open(os.devnull, 'w')

    def run_firmsmith():
        try:
            LOG.info(" ".join(args))
            process = subprocess.Popen(args, stdout=devnull, stderr=subprocess.PIPE)
            (stdoutdata, stderrdata) = process.communicate()
            if process.returncode != None and process.returncode != 0:
                raise CalledProcessError(process.returncode, stderrdata)
        except TimeoutError as e:
            process.kill()
            raise e

    set_timeout(5, run_firmsmith)

# Cparser

def get_cparser_optimizations():
    optimizations = [] # ['-O3', '-Os', '-Og']
    LOG.debug("get cparser optimizations: %s --help-optimization" % CPARSER_BIN)
    output = subprocess.check_output([CPARSER_BIN, '--help-optimization'])
    lines = output.split('\n')
    for line in lines:
        if line.strip() == '':
            continue
        optimization = line.strip().split()[0]
        if optimization.startswith('-f') and \
            not optimization.startswith('-fno-') and \
            optimization != '-fshape-blocks' and \
            optimization != '-foccults' and \
            optimization.find('-fverify') == -1 and \
            optimization.find('-fdump') == -1:
            optimizations.append(optimization)
    return optimizations

def get_cparser_version():
    optimizations = []
    LOG.debug("get cparser version: %s --version" % CPARSER_BIN)
    output = subprocess.check_output([CPARSER_BIN, '--version'])
    return output

def get_cparser_launch_info(args):
    launch_info = lldb.SBLaunchInfo(args)
    return launch_info

def get_common_stacktrace(stacktraces):
    common = []
    indexes = map(lambda x: len(x) - 1, stacktraces)

    def __remove_frame_number(x):
        return x[x.find(': ') + 2:]

    while True:
        same = None
        for i in range(0, len(stacktraces)):
            frame = __remove_frame_number(stacktraces[i][indexes[i]])
            if same == None:
                same = frame
            elif same != frame:
                return common

            # Hack to fix double frame listing bug
            while __remove_frame_number(stacktraces[i][indexes[i]]) == frame:
                indexes[i] -= 1
                if indexes[i] < 0:
                    return common

        common.append(same)

# LLDB Debugging

class NoCrashException(Exception):
    pass

def debug_timeout(debugger, args):
    target = get_debugger_target(debugger)
    launch_info = get_cparser_launch_info(args)
    lldb_error = lldb.SBError()
    process = target.Launch(launch_info, lldb_error)

    debug_points = []
    start_time = time.time()
    n_stops = 0
    for (state, event) in yield_process_events(debugger, process, n_stops=3):
        if state == lldb.eStateInvalid:
            raise Exception("invalid lldb state")
        elif state == lldb.eStateExited:
            debug_point = DebugPoint()
            debug_point.runtime += time.time()
            debug_point.stacktrace_frames = ['DID NOT CRASH IN DEBUGGER']
            debug_points.append(debug_point)
        elif state == lldb.eStateStopped:
            n_stops += 1
            debug_point = DebugPoint()
            debug_point.runtime = time.time() - start_time
            debug_point.stacktrace_frames = get_stacktrace_frames(process.threads[0])
            debug_points.append(debug_point)

            if n_stops == 3:
                ci = debugger.GetCommandInterpreter()
                res = lldb.SBCommandReturnObject()
                command = 'expr dump_all_ir_graphs("%s")' % \
                    (current_report.strid + '-last_stop')
                ci.HandleCommand(command, res)
    return debug_points


def debug_abort(debugger, args):
    return debug_timeout(debugger, args)


def check_ir_graph(debugger, report):
    args = [CPARSER_BIN, '%s/%s.ir' % (REPORT_DIR, report.strid), '-O0', '-m32']

    devnull = open(os.devnull, 'w')
    def run_cparser(args):
        try:
            LOG.info(" ".join(args))
            process = subprocess.Popen(args, stdout=devnull, stderr=subprocess.PIPE)
            (stdoutdata, stderrdata) = process.communicate()
            if process.returncode != None and process.returncode != 0:
                raise CalledProcessError(process.returncode, stderrdata)
        except TimeoutError as e:
            process.kill()
            raise e

    print_debug("_", end="")

    def check_opts(opts):
        record = DebugRecord()
        record.args = args[1:] + opts
        try:
            print_debug(".", end="")
            set_timeout(10, lambda: run_cparser(args + opts))
            report.successes.append(record)
            return True
        except TimeoutError:
            print_debug('T', end='')
            record.debug_points = debug_timeout(debugger, (args + opts)[1:])
            record.timeout = True
            report.timeouts.append(record)
        except CalledProcessError as e:
            try:
                print_debug('A', end='')
                record.debug_points = debug_abort(debugger, (args + opts)[1:])
                record.returncode = e.returncode
                record.stderrdata = e.stderrdata.strip()
                report.aborts.append(record)
            except NoCrashException:
                LOG.warning("NoCrashException")
                LOG.warning("\tFirmsmith arguments: ", report.args)
                LOG.warning("\tCparser arguments:   ", " ".join(record.args))
        finally:
            report.records.append(record)
        return False

    def populate_opts(opts):
        result = []
        for opt in opts:
            opt = opt.replace('<size>', str(random.randint(1, 10)))
            opt = opt.replace('<value>', str(random.randint(1, 10)))
            result.append(opt)
        return result

    global fuzzer_options
    for opts in fuzzer_options["cparser_options"]:
        check_opts(populate_opts(opts.split()))

def fuzz(n):
    global fuzzer_options
    global current_report
    debugger = get_debugger()
    for i in range(n):
        for firmsmith_option in fuzzer_options['firmsmith_options']:
            report = Report()
            current_report = report

            args = get_firmsmith_random_args()
            args.update({'strid': report.strid})
            report.args = get_firmsmith_args_as_string(args) + ' ' + firmsmith_option
            try:
                firmsmith_generate_ir_graph(report.args)
                LOG.info("mv *%s.{vcg,ir} %s" % (report.strid, REPORT_DIR))
                subprocess.call('bash -c "mv *%s.{vcg,ir} %s"' % (report.strid, REPORT_DIR), shell=True)

                check_ir_graph(debugger, report)
                if report.is_bug_report():
                    identifier = report.get_identifier()
                    filename = REPORT_DIR + '/' + report.strid + '.txt'
                    with open(filename, 'w') as report_file:
                        report_file.write(str(report).replace('bugreports', 'bugreports/' + identifier))
                        print("\nReport was written to %s (%d timeouts, %d aborts, %d successes)"  % \
                            (filename.replace('reports/','reports/'+identifier+'/'), len(report.timeouts), len(report.aborts), len(report.successes)))
                    command = """bash -c '
                        REPORT_DIR=%s;
                        STRID=%s;
                        CATEGORY=%s;
                        mv $STRID-last_stop.vcg $REPORT_DIR &>/dev/null;
                        cd $REPORT_DIR;
                        zip $STRID.zip *$STRID* &> /dev/null
                        mkdir -p $CATEGORY
                        mv *$STRID* $CATEGORY
                    '""" % (REPORT_DIR, report.strid, identifier)
                    LOG.info(command)
                    subprocess.call(command, shell=True)
                else:
                    LOG.info("rm %s/*%s.{vcg,ir}" % (REPORT_DIR, report.strid))
                    subprocess.call('bash -c "rm %s/*%s.{vcg,ir}"' % (REPORT_DIR, report.strid), shell=True)

            except CalledProcessError, TimeoutError:
                LOG.error("Could not generate ir graph with arguments %s" % \
                    report.args)


if __name__ == '__main__':
    # Defaults
    #default_cparser_options = map(lambda x: x, get_cparser_optimizations())
    default_cparser_options = filter(
        lambda x: x not in ['-fdeconv', '-freassociation', '-fthread-jumps', '-fcombo'],
        get_cparser_optimizations()
    )
    default_firmsmith_options = [
        # Few functions, complex callgraph
        '-ffunc-cycles --nfuncs 5 --func-maxcalls 5 --cfg-size 10 --cfb-size 10',
        # Many functions, no cycles, small cfg, cfb
        '-fno-func-cycles --nfuncs 100 --func-maxcalls 2 --cfg-size 5 --cfb-size 5',
        # Few functions, big size, no memorry operations
        '-fno-memory --nfuncs 3 --func-maxcalls 2 --cfg-size 100 --cfb-size 100'
    ]
    # Setup parser
    parser = argparse.ArgumentParser(description = 'FirmSmith Fuzzer')
    parser.add_argument('--cparser-options',
        action='append', help='cparser options to apply on generated graphs')
    parser.add_argument('--firmsmith-options',
        action='append', help='firmsmith options for graph generation')
    parser.add_argument('--debug', action='store_true', default=False,
         help='enable debugging output')

    LOG.debug(sys.argv)
    fuzzer_options = vars(parser.parse_args(sys.argv[1:]))
    if fuzzer_options['cparser_options'] == None:
        fuzzer_options['cparser_options'] = default_cparser_options
    if fuzzer_options['firmsmith_options'] == None:
        fuzzer_options['firmsmith_options'] = default_firmsmith_options
    LOG.debug(fuzzer_options['firmsmith_options'])
    LOG.debug(fuzzer_options['cparser_options'])
    if (fuzzer_options['debug']):
        LOG.setLevel(logging.DEBUG)
    now = datetime.now()
    fuzz(1000)

