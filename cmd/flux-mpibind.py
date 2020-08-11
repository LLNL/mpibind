import os
import sys
import logging
import argparse
import json
from itertools import chain

from _mpibind._mpibind import ffi, lib
from .cffiwrapper.mpibind_wrapper import MpibindWrapper

import flux
from flux import job
from flux.job import JobspecV1, JobspecV2
from flux import util
from flux import debugged


class RunCmd:
    """
    Run multiple jobs on a node using mpibind to divide resources
    """

    def __init__(self, **kwargs):
        self.parser = self.run_parser(kwargs)
        self.mpibind_py = MpibindWrapper()
    
    def get_parser(self):
        return self.parser

    @staticmethod
    def run_parser(exclude_io=False):
        """
        Parse Command Line Arguments for jobspec creation
        """
        parser = argparse.ArgumentParser(add_help=False)
        parser.add_argument(
            "--dry-run",
            action="store_true",
            help="print jobspec instead of submitting jobs",
        )
        parser.add_argument(
            "-t",
            "--time-limit",
            type=str,
            metavar="FSD",
            help="Time limit in Flux standard duration, e.g. 2d, 1.5h",
        )
        parser.add_argument(
            "--priority",
            help="Set job priority (0-31, default=16)",
            type=int,
            metavar="N",
            default=16,
        )
        parser.add_argument(
            "--setattr",
            action="append",
            help="Set job attribute ATTR to VAL (multiple use OK)",
            metavar="ATTR=VAL",
        )
        parser.add_argument(
            "--hw-threads-per-core",
            "--smt",
            type=int,
            metavar="N",
            help="Number of threads to allocate per core",
            default=1
        )
        parser.add_argument(
            "-G",
            "--greedy",
            type=int,
            choices={0, 1},
            help="Toggle the greedy option for mpibind",
            default=0
        )
        parser.add_argument(
            "-g",
            "--gpu-optim",
            type=int,
            choices={0, 1},
            help="Toggle the gpu optimization option for mpibind",
            default=0
        )
        parser.add_argument(
            "--job-name",
            type=str,
            help="Set an optional name for job to NAME",
            metavar="NAME",
        )
        parser.add_argument(
            "--command", 
            help="Job command and arguments",
            action="append",
            default=[]
        )
        parser.add_argument(
            "--jobspec-dir", 
            help="Directory to place jospecs in",
            default='.'
        )
        parser.add_argument(
            "--input",
            type=str,
            help="Redirect job stdin from FILENAME, bypassing KVS"
            if not exclude_io
            else argparse.SUPPRESS,
            metavar="FILENAME",
        )
        parser.add_argument(
            "--output",
            type=str,
            help="Redirect job stdout to FILENAME, bypassing KVS"
            if not exclude_io
            else argparse.SUPPRESS,
            metavar="FILENAME",
        )
        parser.add_argument(
            "--error",
            type=str,
            help="Redirect job stderr to FILENAME, bypassing KVS"
            if not exclude_io
            else argparse.SUPPRESS,
            metavar="FILENAME",
        )
        parser.add_argument(
            "-l",
            "--label-io",
            action="store_true",
            help="Add rank labels to stdout, stderr lines"
            if not exclude_io
            else argparse.SUPPRESS,
        )
        parser.add_argument(
            "-o",
            "--setopt",
            action="append",
            help="Set shell option OPT. An optional value is supported with"
            + " OPT=VAL (default VAL=1) (multiple use OK)",
            metavar="OPT",
        )
        parser.add_argument(
            "--debug-emulate", action="store_true", help=argparse.SUPPRESS
        )
        parser.add_argument(
            "--flags",
            action="append",
            help="Set comma separated list of job submission flags. Possible "
            + "flags:  debug, waitable",
            metavar="FLAGS",
        )
        return parser

    def submit(self, args):
        """
        Submit job, constructing jobspec from args.
        Returns jobid.
        """
        flux_handle = flux.Flux()
        jids = []
        jobspecs = self.init_jobspec(args)

        for js in jobspecs:
            js.cwd = os.getcwd()
            js.environment = dict(os.environ)

            if args.time_limit is not None:
                js.duration = args.time_limit

            if args.job_name is not None:
                js.setattr("system.job.name", args.job_name)

            if args.input is not None:
                js.stdin = args.input

            if args.output is not None and args.output not in ["none", "kvs"]:
                js.stdout = args.output
            
            if args.label_io:
                js.setattr_shell_option("output.stdout.label", True)

            if args.error is not None:
                js.stderr = args.error
                if args.label_io:
                    js.setattr_shell_option("output.stderr.label", True)

            if args.setopt is not None:
                for keyval in args.setopt:
                    # Split into key, val with a default for 1 if no val given:
                    key, val = (keyval.split("=", 1) + [1])[:2]
                    try:
                        val = json.loads(val)
                    except (json.JSONDecodeError, TypeError):
                        pass
                    
                    js.setattr_shell_option(key, val)

            if args.debug_emulate:
                debugged.set_mpir_being_debugged(1)

            if debugged.get_mpir_being_debugged() == 1:
                # if stop-tasks-in-exec is present, overwrite
                js.setattr_shell_option("stop-tasks-in-exec", json.loads("1"))

            if args.setattr is not None:
                for keyval in args.setattr:
                    tmp = keyval.split("=", 1)
                    if len(tmp) != 2:
                        raise ValueError("--setattr: Missing value for attr " + keyval)
                    key = tmp[0]
                    try:
                        val = json.loads(tmp[1])
                    except (json.JSONDecodeError, TypeError):
                        val = tmp[1]
                    js.setattr(key, val)

            arg_debug = False
            arg_waitable = False
            if args.flags is not None:
                for tmp in args.flags:
                    for flag in tmp.split(","):
                        if flag == "debug":
                            arg_debug = True
                        elif flag == "waitable":
                            arg_waitable = True
                        else:
                            raise ValueError("--flags: Unknown flag " + flag)

            if args.dry_run:
                print(js.dumps(), file=sys.stdout)
            else:
                jids.append(
                    job.submit(
                        flux_handle,
                        js.dumps(),
                        priority=args.priority,
                        waitable=arg_waitable,
                        debug=arg_debug,
                    )
                )

        return jids

        
    def init_jobspec(self, args):
        """
        generate
        """
        num_jobs = len(args.command)
        if num_jobs < 1:
            print("Please specify at least one job to run")
            raise SystemExit

        handle = ffi.new("struct mpibind_t *")

        print(
            f"""
            You requested:
            jobs: {num_jobs}
            smt: {args.hw_threads_per_core}
            greedy: {args.greedy}
            gput_optim: {args.gpu_optim}
            """

        )

        self.mpibind_py.set_ntasks(handle, num_jobs)
        self.mpibind_py.set_greedy(handle, args.greedy)
        self.mpibind_py.set_gpu_optim(handle, args.gpu_optim)
        self.mpibind_py.set_smt(handle, args.hw_threads_per_core)
        self.mpibind_py.mpibind(handle)

        self.mpibind_py.print_mapping(handle)
        
        mapping = self.mpibind_py.get_mapping(handle)
        print(mapping.counts)
        jobspecs = []
        for job, counts in zip(args.command, mapping.counts.values()):
            jobspecs.append(
                JobspecV2.from_command(
                    job.split(" "),
                    num_nodes=1,
                    num_tasks=1,
                    cores_per_task=counts['cpus'],
                    hw_threads_per_core= int(counts['thds'] / counts['cpus']),
                    gpus_per_task=counts['gpus'],
                )
            )

        return jobspecs

    def write_jobspec(self, args, jobspecs):
        for idx, jobspec in enumerate(jobspecs):
            with open(os.path.join(args.jobspec_dir, "js_{0:}.json".format(idx)), 'w') as f:
                f.write(jobspec)

    def main(self, args):
        jids = self.submit(args)

        for jid in jids:
            print(jid)
    
def main():
    parser = argparse.ArgumentParser(prog="jobspec")
    subparsers = parser.add_subparsers(
        title="supported subcommands", description="", dest="subcommand"
    )
    subparsers.required = True

    # run
    run = RunCmd()
    jobspec_run_parser_sub = subparsers.add_parser(
        "run",
        parents=[run.get_parser()],
        help="generate jobspec for multiple jobs using mpibind",
    )
    jobspec_run_parser_sub.set_defaults(func=run.main)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()