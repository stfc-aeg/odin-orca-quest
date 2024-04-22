config_template = """[
    {
        "fr_setup": {
            "fr_ready_cnxn":"tcp://0.0.0.0:6969",
            "fr_release_cnxn":"tcp://0.0.0.0:6970"
        }
    },
    {
        "plugin": {
            "load": {
                "index":"hibirdsdpdk",
                "name":"HibirdsDpdkPlugin",
                "library":"./lib/libHibirdsDpdkPlugin.so"
            }
        }
    },
    {
        "plugin": {
            "load": {
                "index":"blosc",
                "name":"BloscPlugin",
                "library":"./lib/libBloscPlugin.so"
            }
        }
    },
    {
        "plugin": {
            "load": {
                "index":"hdf",
                "name":"FileWriterPlugin",
                "library":"./lib/libHdf5Plugin.so"
            }
        }
    },
    {
        "plugin": {
            "connect": {
                "index":"hibirdsdpdk",
                "connection":"frame_receiver"
            }
        }
    },
    {
        "plugin": {
            "connect": {
                "index":"hdf",
                "connection":"hibirdsdpdk"
            }
        }
    },
    {
        "hibirdsdpdk": {
            "dpdk_process_rank": {{ proc_rank }},
            "num_secondary_processes": {{ secondary_procs }},
            "shared_buffer_size": {{ shared_buf_size }},
            "dpdk_eal" : {
                "corelist": "{{ corelist }}",
                "loglevel": "debug",
                "allowdevice": "0000:2b:00.0",
                "proc-type": "{{ proc_type }}",
                "file-prefix":"odin-data"
            },
            "worker_cores": {
                {% if 'packet_rx' in include_cores %}
                "packet_rx": {
                    "core_name": "PacketRxCore",
                    "num_cores": {{ rx_cores }},
                    "rx_ports": [1234, 1235],
                    "device_ip": "10.0.100.6",
                    "rx_burst_size": 128,
                    "fwd_ring_size": 32786,
                    "release_ring_size": 32768,
                    "rx_queue_id" : 0,
                    "tx_queue_id" : 0,
                    "max_packet_tx_retries": 64,
                    "max_packet_queue_retries": 64
                },
                {% endif %}
                {% if 'packet_processor' in include_cores %}
                "packet_processor": {
                    "core_name": "PacketProcessorCore",
                    "num_cores": {{ ppc_cores }},
                    "connect": "{{ ppc_connect }}",
                    "frame_timeout": 1000
                },
                {% endif %}
                {% if 'frame_builder' in include_cores %}
                "frame_builder": {
                    "core_name": "FrameBuilderCore",
                    "num_cores": {{ fbc_cores }},
                    "connect": "{{ fbc_connect }}",
                    "secondary_fanout" : {{ fbc_fanout|lower }}
                },
                {% endif %}
                {% if 'frame_compressor' in include_cores %}
                "frame_compressor": {
                    "core_name": "FrameCompressorCore",
                    "num_cores": {{ fcc_cores }},
                    "connect": "{{ fcc_connect }}",
                    "blosc_clevel" : 4,
                    "blosc_doshuffle" : 2,
                    "blosc_compcode" : 1,
                    "blosc_blocksize" : 0,
                    "blosc_num_threads" : 1,
                    "secondary_fanout" : {{ fcc_fanout|lower }}
                },
                {% endif %}
                {% if 'frame_wrapper' in include_cores %}
                "frame_wrapper": {
                    "core_name": "FrameWrapperCore",
                    "num_cores": {{ fwc_cores }},
                    "connect": "{{ fwc_connect }}"
                }
                {% endif %}
            }
        }
    },
    {
        "hdf":
        {
            "dataset":
            {
                "dummy":
                {
                    "datatype":"uint16",
                    "dims":[1000, 1000],
                    "chunks": [1, 1000, 1000],
                    {% if 'frame_compressor' in include_cores %}
                    "compression": "blosc",
                    "blosc_compressor": 1,
                    "blosc_level": 4,
                    "blosc_shuffle": 2
                    {% else %}
                    "compression": "none"
                    {% endif %}
                }
            },
            "file":
            {
                "path":"/mnt/hptblock0n8p/odin-data"
            },
            "frames":60000,
            "acquisition_id":"test_1",
            "write":true,
            "timeout_timer_period":10000,
            "process":
            {
                "number": {{ HDF5_number }},
                "rank": {{ HDF5_rank }}
            }
        }
    }

]"""
