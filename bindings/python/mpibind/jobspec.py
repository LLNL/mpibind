import os
import sys
import logging
import argparse
import json
from itertools import chain
from _mpibind._mpibind import ffi, lib
from .mpibind_wrapper import MpibindWrapper
from flux import job
from flux.job import JobspecV1, JobspecV2


class CreateCmd:
    """
    Create a valid jobspec as a proof of concept for mpibind at the job level
    """

    def __init__(self, **kwargs):
        self.parser = self.create_parser(kwargs)
        self.mpibind_py = MpibindWrapper()
    
    def get_parser(self):
        return self.parser

    @staticmethod
    def create_parser(exclude_io=False):
        """
        Parse Command Line Arguments for jobspec creation
        """
        parser = argparse.ArgumentParser(add_help=False)
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
            "--job-name",
            type=str,
            help="Set an optional name for job to NAME",
            metavar="NAME",
        )
        parser.add_argument(
            "--setattr",
            action="append",
            help="Set job attribute ATTR to VAL (multiple use OK)",
            metavar="ATTR=VAL",
        )
        parser.add_argument(
            "-T",
            "--hw-threads-per-core",
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
            "--command", 
            help="Job command and arguments",
            action="append",
            default=[]
        )
        return parser
    
    def init_jobspec(self, args):
        """
        generate
        """
        num_jobs = len(args.command)
        if num_jobs < 1:
            print("Please specify at least one job to run")
            raise SystemExit

        handle = ffi.new("struct mpibind_t *")
        self.mpibind_py.set_ntasks(handle, num_jobs)
        self.mpibind_py.set_greedy(handle, args.greedy)
        self.mpibind_py.set_gpu_optim(handle, args.gpu_optim)
        self.mpibind_py.set_smt(handle, args.hw_threads_per_core)
        self.mpibind_py.mpibind(handle)

        #TODO PLACE HOLDER FOR GETTING MAPPING
        mpibind_map = None

        jobspecs = []
        for idx, job in enumerate(args.command):
            jobspecs.append(
                JobspecV2.from_command(
                    args.command[idx],
                    num_tasks=args.ntasks,
                    cores_per_task=args.cores_per_task,
                    hw_threads_per_core=args.hw_threads_per_core,
                    gpus_per_task=args.gpus_per_task,
                    num_nodes=args.nodes,
                )
            )

    def main(self, args):
        return self.init_jobspec(args)
    
def main():
    parser = argparse.ArgumentParser(prog="jobspec")
    subparsers = parser.add_subparsers(
        title="supported subcommands", description="", dest="subcommand"
    )
    subparsers.required = True

    # create
    create = CreateCmd()
    jobspec_create_parser_sub = subparsers.add_parser(
        "create",
        parser=[create.get_parser()],
        help="generate jobspec for multiple jobs using mpibind",
    )
    jobspec_create_parser_sub.set_defaults(func=create.main)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()