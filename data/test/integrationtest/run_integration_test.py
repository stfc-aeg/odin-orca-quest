import argparse
import json
import sys
import subprocess
import shlex
import time
from threading import Timer
import re
import jinja2
from jinja2 import Template

from templates import config_template


class IntegrationTest(object):

    def __init__(self):
        self.used_cores = [0]
        parser = argparse.ArgumentParser(prog="IntegrationTest",
                                         description="IntegrationTest - run PERCIVAL frame receiver integration test")

        parser.add_argument('--frames', '-n', type=int, default=60000,
                            help='select number of frames to transmit in test')
        parser.add_argument('--interval', '-i', type=float, default=0.1,
                            help='set frame output interval in seconds')
        parser.add_argument('--timeout', '-t', type=float, default=5.0,
                            help="set timeout for process completion")
        parser.add_argument('--config', '-c', type=str, default='',
                            help='set the location of a run config')
        parser.add_argument('--rxcores', '-rxc', type=int, default=1,
                            help='set the number of Packet Rx Cores in the Primary process')
        parser.add_argument('--ppcores', '-ppc', type=int, default=4,
                            help='set the number of Packet Processor Cores in the Primary process')
        parser.add_argument('--fbcores', '-fbc', type=int, default=2,
                            help='set the number of Frame Builder Cores in the Primary process')
        parser.add_argument('--fccores', '-fcc', type=int, default=4,
                            help='set the number of Frame Compressor Cores in the Primary process')
        parser.add_argument('--fwcores', '-fwc', type=int, default=1,
                            help='Number of Frame Wrapper Cores in the Primary process.')
        parser.add_argument('--secondary', '-s', type=int, default=0,
                            help='set the number of secondary processes to run')
        parser.add_argument('--shared_buf_size', type=int, default=103079215104,
                            help='Set the shared buffer size.')
        parser.add_argument('--ppc_connect', type=str, default="packet_rx",
                            help='Connection for Packet Processor Core.')
        parser.add_argument('--fbc_connect', type=str, default="packet_processor",
                            help='Connection for Frame Builder Core.')
        parser.add_argument('--fcc_connect', type=str, default="frame_builder",
                            help='Connection for Frame Compressor Core.')
        parser.add_argument('--fwc_connect', type=str, default="frame_compressor",
                            help='Connection for Frame Wrapper Core.')
        parser.add_argument('--file_path', type=str, default="config.json",
                            help='Path to save the generated config file.')

        args = parser.parse_args()

        self.frames = args.frames
        self.interval = args.interval
        self.timeout = args.timeout
        self.config = args.config
        self.rxcores = args.rxcores
        self.ppcores = args.ppcores
        self.fbcores = args.fbcores
        self.fccores = args.fccores
        self.secondary = args.secondary
        self.total_procs = args.secondary + 1
        self.shared_buf_size = args.shared_buf_size
        self.ppc_connect = args.ppc_connect
        self.fbc_connect = args.fbc_connect
        self.fcc_connect = args.fcc_connect
        self.fwc_cores = args.fwcores
        self.fwc_connect = args.fwc_connect
        self.file_path = args.file_path

        self.include_cores = []
        if self.rxcores > 0:
            self.include_cores.append('packet_rx')
        if self.ppcores > 0:
            self.include_cores.append('packet_processor')
        if self.fbcores > 0:
            self.include_cores.append('frame_builder')
            self.fbc_fanout = True
        if self.fccores > 0:
            self.include_cores.append('frame_compressor')
            self.fcc_fanout = True
            self.fbc_fanout = False
        if self.fwc_cores > 0:
            self.include_cores.append('frame_wrapper')


    def run(self):
        self.generate_config_file()

    def generate_config_file(self):

        # Create Jinja2 template object
        template = Template(config_template)

        total_procs = self.secondary + 1

        HDF5_rank = 0
        proc_rank = 0

        for proc_num in range(total_procs):

            # If it's the first iteration, set for primary
            if proc_num == 0:
                proc_type = "Primary"

                rendered_config = template.render(
                    proc_rank=proc_rank,
                    secondary_procs=self.secondary,
                    shared_buf_size=self.shared_buf_size,
                    corelist=self.generate_corelist(self.fwc_cores + self.fccores + self.fbcores + self.ppcores + 2),
                    proc_type=proc_type,
                    include_cores=self.include_cores,
                    rx_cores = self.rxcores,
                    ppc_cores=self.ppcores,
                    ppc_connect=self.ppc_connect,
                    fbc_cores=self.fbcores,
                    fbc_connect=self.fbc_connect,
                    fbc_fanout=self.fbc_fanout,
                    fcc_cores=self.fccores,
                    fcc_connect=self.fcc_connect,
                    fcc_fanout=self.fcc_fanout,
                    fwc_cores=self.fwc_cores,
                    fwc_connect=self.fwc_connect,
                    HDF5_number=self.total_procs,
                    HDF5_rank=HDF5_rank
                )

            else:
                proc_type = "Secondary"
                HDF5_rank += 1
                proc_rank += 1

                rendered_config = template.render(
                    proc_rank=proc_rank,
                    secondary_procs=self.secondary,
                    shared_buf_size=self.shared_buf_size,
                    corelist=self.generate_corelist(2),
                    proc_type=proc_type,
                    include_cores=self.include_cores,
                    rx_cores = 0,
                    ppc_cores=0,
                    ppc_connect=self.ppc_connect,
                    fbc_cores=0,
                    fbc_connect=self.fbc_connect,
                    fbc_fanout=self.fbc_fanout,
                    fcc_cores=0,
                    fcc_connect=self.fcc_connect,
                    fcc_fanout=self.fcc_fanout,
                    fwc_cores=self.fwc_cores,
                    fwc_connect=self.fwc_connect,
                    HDF5_number=self.total_procs,
                    HDF5_rank=HDF5_rank
                )

            # use a unique file name based on the proc number.
            file_name = self.file_path.replace('.json', f'_{proc_num}.json')

            # Write the rendered template to a file
            with open(file_name, 'w') as outfile:
                json_obj = json.loads(rendered_config)
                json.dump(json_obj, outfile, indent=4)

    def generate_corelist(self, n):
        """Generate a corelist of n cores."""
        corelist = [0]
        core = 1  # Start from core 1, since core 0 is already in the list

        while len(corelist) < n:
            if core not in self.used_cores:
                corelist.append(core)
                self.used_cores.append(core)
            core += 1

        return ",".join(map(str, corelist))

    def launch_processes(self):
        # This function will launch a process for each generated config
        config_files = [self.file_path.replace('.json', f'_{i}.json') for i in range(self.total_procs)]

        processes = []

        for config_file in config_files:
            cmd = f'bin/frameProcessor --ctrl tcp://127.0.0.1:{5001 + len(processes)} --logconfig config/data/fp_log4cxx.xml --json {config_file}'
            processes.append(subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE))
            if (len(processes) == 1):
                time.sleep(10)

        for process in processes:
            self.monitor_process(process)

        

    def monitor_process(self, process):
        while True:
            output = process.stdout.readline().decode()
            
            # Check if process ended
            if output == '' and process.poll() is not None:
                break
            
            # Check for the first line of interest and capture the entire line
            if "FP.PacketRxCore INFO  - PacketRxCore : 0 First packet:" in output:
                print(f"Captured line: {output.strip()}")  # Remove newline characters using strip()
            
            # Check for the closing file info and capture the entire line
            if "FP.Acquisition INFO  - Closing file /mnt/hptblock0n8p/odin-data/test_1_00000" in output:
                print(f"Captured line: {output.strip()}")  # Remove newline characters using strip()

                # If you also want to capture the filename separately
                match = re.search(r"Closing file (.+\.h5)", output)
                if match:
                    h5_filename = match.group(1)
                    print(f"Extracted filename: {h5_filename}")
                    

                input("Press Enter to kill processes...")
                # Kill the process and break out of loop
                process.kill()
                break


if __name__ == '__main__':
    integration_test = IntegrationTest()
    integration_test.run()
    integration_test.launch_processes()
    sys.exit(0)