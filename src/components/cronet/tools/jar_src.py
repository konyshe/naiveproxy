#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import zipfile

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
from util import build_utils  # pylint: disable=wrong-import-position

JAVA_PACKAGE_PREFIX = 'org/chromium/'


def main():
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument(
      '--excluded-classes',
      help='A list of .class file patterns to exclude from the jar.')
  parser.add_argument(
      '--src-search-dirs',
      action='append',
      help='A list of directories that should be searched'
      ' for the source files.')
  parser.add_argument(
      '--src-files', action='append', help='A list of source files to jar.')
  parser.add_argument(
      '--src-jars',
      action='append',
      help='A list of source jars to include in addition to source files.')
  parser.add_argument(
      '--src-list-files',
      action='append',
      help='A list of files that contain a list of sources,'
      ' e.g. a list of \'.sources\' files generated by GN.')
  parser.add_argument('--jar-path', help='Jar output path.', required=True)

  options = parser.parse_args()

  src_jars = []
  for gn_list in options.src_jars:
    src_jars.extend(build_utils.ParseGnList(gn_list))

  src_search_dirs = []
  for gn_src_search_dirs in options.src_search_dirs:
    src_search_dirs.extend(build_utils.ParseGnList(gn_src_search_dirs))

  src_list_files = []
  if options.src_list_files:
    for gn_src_list_file in options.src_list_files:
      src_list_files.extend(build_utils.ParseGnList(gn_src_list_file))

  src_files = []
  for gn_src_files in options.src_files:
    src_files.extend(build_utils.ParseGnList(gn_src_files))

  # Add files from --source_list_files
  for src_list_file in src_list_files:
    with open(src_list_file, 'r') as f:
      src_files.extend(f.read().splitlines())

  # Preprocess source files by removing any prefix that comes before
  # the Java package name.
  for i, s in enumerate(src_files):
    prefix_position = s.find(JAVA_PACKAGE_PREFIX)
    if prefix_position != -1:
      src_files[i] = s[prefix_position:]

  excluded_classes = []
  if options.excluded_classes:
    classes = build_utils.ParseGnList(options.excluded_classes)
    excluded_classes.extend(f.replace('.class', '.java') for f in classes)

  predicate = None
  if excluded_classes:
    predicate = lambda f: not build_utils.MatchesGlob(f, excluded_classes)

  # Create a dictionary that maps every source directory
  # to source files that it contains.
  dir_to_files_map = {}
  # Initialize the map.
  for src_search_dir in src_search_dirs:
    dir_to_files_map[src_search_dir] = []
  # Fill the map.
  for src_file in src_files:
    number_of_file_instances = 0
    for src_search_dir in src_search_dirs:
      target_path = os.path.join(src_search_dir, src_file)
      if os.path.isfile(target_path):
        number_of_file_instances += 1
        if not predicate or predicate(src_file):
          dir_to_files_map[src_search_dir].append(target_path)
    if (number_of_file_instances > 1):
      raise Exception(
          'There is more than one instance of file %s in %s'
          % (src_file, src_search_dirs))
    if (number_of_file_instances < 1):
      raise Exception(
          'Unable to find file %s in %s' % (src_file, src_search_dirs))

  # Jar the sources from every source search directory.
  with build_utils.AtomicOutput(options.jar_path) as o, \
      zipfile.ZipFile(o, 'w', zipfile.ZIP_DEFLATED) as z:
    for src_search_dir in src_search_dirs:
      subpaths = dir_to_files_map[src_search_dir]
      if subpaths:
        build_utils.DoZip(subpaths, z, base_dir=src_search_dir)
      else:
        raise Exception(
            'Directory %s does not contain any files and can be'
            ' removed from the list of directories to search' % src_search_dir)

    # Jar additional src jars
    if src_jars:
      build_utils.MergeZips(z, src_jars, compress=True)

  if options.depfile:
    deps = []
    for sources in dir_to_files_map.values():
      deps.extend(sources)
    # Srcjar deps already captured in GN rules (no need to list them here).
    build_utils.WriteDepfile(options.depfile, options.jar_path, deps)

if __name__ == '__main__':
  sys.exit(main())
