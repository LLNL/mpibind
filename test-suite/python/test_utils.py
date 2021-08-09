import json
import mpibind
import unittest
import re

def parse_expected(answer_file):
    """
    Parse parameters and answers from the expected file

    :param answer_file: file path of the expected file
    :type answer_file: string or path-like
    :return: test information for each of the tests in the answer file
    :rtype: list of dictionaries
    """
    line_types = [
        'description',
        'thread_mapping',
        'cpu_mapping',
        'gpu_mapping'
    ]
    test_info = []
    cur_answer = dict()
    type_idx = 0
    with open(answer_file, 'r') as f:
        for line in f.readlines():
            line = line.strip() 
            if not line:
                continue
            if line[0] == '#' and 'params' not in line:
                continue
            
            if 'params' in line:
                json_string = line.replace('# ', '')
                cur_answer['params'] = json.loads(json_string)['params']
            else:
                cur_answer[line_types[type_idx]] = line.replace('"', '')
                type_idx += 1

            if type_idx == 4:
                test_info.append(cur_answer)
                type_idx = 0
                cur_answer = dict()

    return test_info

def get_actual(handle, single_test_info):
    """
    Use the test info to paramaterize a mpibind handle
    and then compute a mapping.

    :param handle: the mpibind handle
    :type handle: MpibindHandle
    :param single_test_info: the test information for a single test
    :type single_test_info: dictionary
    :return: 3-tuple of answers
    :rtype: tuple of strings
    """
    handle.ntasks = single_test_info['params']['ntasks']
    handle.nthreads = single_test_info['params']['in_nthreads']
    handle.greedy = single_test_info['params']['greedy']
    handle.gpu_optim = single_test_info['params']['gpu_optim']
    handle.smt = single_test_info['params']['smt']
    handle.restrict_ids = single_test_info['params']['restr_set']
    handle.restrict_type = single_test_info['params']['restrict_type']
    handle.mpibind()

    thread_mapping = ';'.join([str(ele) for ele in handle.nthreads])
    cpu_mapping = ';'.join([handle.get_cpus_ptask(i) for i in range(handle.ntasks)])
    gpu_mapping = ';'.join([','.join(handle.get_gpus_ptask(i)) for i in range(handle.ntasks)])
    return thread_mapping, cpu_mapping, gpu_mapping

def make_test_name(description):
    """
    generate a test name from a description

    :param description: plain english description of a test
    :type description: string
    :return: the generated test name
    :rtype: string
    """
    return 'test_' + re.sub(r'\s+', '_', description.strip().lower())

def test_generator(single_test_info):
    """
    generate a python unit test from the given test info

    :param single_test_info: the test information for a single test
    :type single_test_info: dictionary
    :return: the generated test
    :rtype: function
    """
    def test(self):
        thread_mapping, cpu_mapping, gpu_mapping = get_actual(self.handle, single_test_info)
        self.assertEqual(thread_mapping, single_test_info['thread_mapping'])
        self.assertEqual(cpu_mapping, single_test_info['cpu_mapping'])
        self.assertEqual(gpu_mapping, single_test_info['gpu_mapping'])
    return test

def setup_generator(topology_file):
    """
    generate the setup functino

    :param topology_file: the path to the topology file to use during testing
    :type topology_file: string
    :return: the generated setup function
    :rtype: function
    """
    def setUp(self):
        mpibind.topology_set_xml(topology_file)
        self.handle = mpibind.MpibindHandle()
    return setUp

def teardown_generator():
    """
    generate the teardown function

    :return: the generated teardown function
    :rtype: function
    """
    def tearDown(self):
        self.handle.finalize()
    return tearDown
