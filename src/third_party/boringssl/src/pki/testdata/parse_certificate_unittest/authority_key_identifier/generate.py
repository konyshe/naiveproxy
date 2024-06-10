#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is called without any arguments to re-generate all of the *.pem
files in the script's directory.

The https://github.com/google/der-ascii tools must be in the PATH.
"""

import base64
import subprocess
import os


HEADER = "Generated by %s. Do not edit." % os.path.split(__file__)[1]


def Ascii2Der(txt):
  p = subprocess.Popen(['ascii2der'],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
  stdout_data, stderr_data = p.communicate(txt)
  if p.returncode:
    raise RuntimeError('ascii2der returned %i: %s' % (p.returncode,
                                                      stderr_data))
  return stdout_data


def MakePemBlock(text, der, name):
  b64 = base64.b64encode(der)
  wrapped = '\n'.join(b64[pos:pos + 64] for pos in xrange(0, len(b64), 64))
  return '%s\n\n%s\n-----BEGIN %s-----\n%s\n-----END %s-----' % (
      HEADER, text, name, wrapped, name)


def Generate(path, dertext):
  der = Ascii2Der(dertext)
  data = MakePemBlock(dertext, der, 'AUTHORITY_KEY_IDENTIFIER')
  with open(path, "w") as f:
    f.write(data)


Generate('empty_sequence.pem', 'SEQUENCE {}')

Generate('key_identifier.pem', """
  SEQUENCE {
    [0 PRIMITIVE] { `DEADB00F`}
  }
""")

Generate('issuer_and_serial.pem', """
  SEQUENCE {
    [1] {
      [4] {
        SEQUENCE {
          SET {
            SEQUENCE {
              # commonName
              OBJECT_IDENTIFIER { 2.5.4.3 }
              UTF8String { "Root" }
            }
          }
        }
      }
    }
    [2 PRIMITIVE] { `274f` }
  }
""")

Generate('url_issuer_and_serial.pem', """
  SEQUENCE {
    [1] {
      [6 PRIMITIVE] { "http://example.com" }
    }
    [2 PRIMITIVE] { `274f` }
  }
""")

Generate('key_identifier_and_issuer_and_serial.pem', """
  SEQUENCE {
    [0 PRIMITIVE] { `DEADB00F`}
    [1] {
      [4] {
        SEQUENCE {
          SET {
            SEQUENCE {
              # commonName
              OBJECT_IDENTIFIER { 2.5.4.3 }
              UTF8String { "Root" }
            }
          }
        }
      }
    }
    [2 PRIMITIVE] { `274f` }
  }
""")

Generate('issuer_only.pem', """
  SEQUENCE {
    [1] {
      [4] {
        SEQUENCE {
          SET {
            SEQUENCE {
              # commonName
              OBJECT_IDENTIFIER { 2.5.4.3 }
              UTF8String { "Root" }
            }
          }
        }
      }
    }
  }
""")

Generate('serial_only.pem', """
  SEQUENCE {
    [2 PRIMITIVE] { `274f` }
  }
""")

Generate('invalid_contents.pem', """
  SEQUENCE {
    INTEGER {`1234`}
  }
""")

Generate('invalid_key_identifier.pem', """
  SEQUENCE {
    # Tag and Length for [0 PRIMITIVE], but no data.
    `8004`
  }
""")

Generate('invalid_issuer.pem', """
  SEQUENCE {
    [0 PRIMITIVE] { `DEADB00F`}
    # Tag and Length for [1] {...}, but no data.
    `a104`
  }
""")

Generate('invalid_serial.pem', """
  SEQUENCE {
    [0 PRIMITIVE] { `DEADB00F`}
    [1] {}
    # Tag and Length for [2 PRIMITIVE], but no data.
    `8204`
  }
""")

Generate('extra_contents_after_issuer_and_serial.pem', """
  SEQUENCE {
    [1] {
      [4] {
        SEQUENCE {
          SET {
            SEQUENCE {
              # commonName
              OBJECT_IDENTIFIER { 2.5.4.3 }
              UTF8String { "Root" }
            }
          }
        }
      }
    }
    [2 PRIMITIVE] { `274f` }
    INTEGER {`1234`}
  }
""")

Generate('extra_contents_after_extension_sequence.pem', """
  SEQUENCE {
    [0 PRIMITIVE] { `DEADB00F`}
  }
  INTEGER {`1234`}
""")
