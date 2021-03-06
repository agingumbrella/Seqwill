#!/usr/bin/env python
from __future__ import division, with_statement

"""
load_genomedata: DESCRIPTION
"""

__version__ = "$Revision: 3641 $"

# Copyright 2009 Orion Buske <orion.buske@gmail.com>
# Copyright 2010 Michael Hoffman <mmh1@uw.edu>

from glob import glob
from os import close, extsep
from path import path
from subprocess import call
import sys
from tempfile import mkdtemp, mkstemp

from . import EXT, FILE_MODE_CHROMS, SUFFIX
from ._load_seq import load_seq
from ._open_data import open_data
from ._load_data import DEFAULT_CHUNK_SIZE, load_data
from ._close_data import close_data

def die(msg="Unexpected error."):
    print >>sys.stderr, msg
    sys.exit(1)

def load_genomedata(gdfilename, tracks=None, seqfilenames=None, mode=None,
                    chunk_size=DEFAULT_CHUNK_SIZE, verbose=False):
    """Loads Genomedata collection with given data

    gdfilename: name of Genomedata archive to create
    tracks: a list of tracks to add, with a (track_name, track_filename) tuple
      for each track to add
    seqfilenames: list of filenames containing sequence data to add
    mode: "dir", "file", or None (decide based upon number of sequences)
    """

    gdpath = path(gdfilename).expand()
    try:
        if mode is None:
            if (seqfilenames is not None
                and len(seqfilenames) > FILE_MODE_CHROMS):
                mode = "file"
            else:
                mode = "dir"

        if mode == "dir":
            # Generate hdf5 data in temporary directory and copy out when done
            tempdatadir = mkdtemp(prefix=(EXT + extsep))
            tempdatapath = path(tempdatadir)
            isdir = True
        elif mode == "file":
            tempdatafile, tempdatafilename = mkstemp(suffix=SUFFIX)
            tempdatapath = path(tempdatafilename)
            # Close the file _descriptor_ (not a file object)
            close(tempdatafile)
            # Delete the file to allow load_seq to create it
            tempdatapath.remove()
            isdir = False
        else:
            raise ValueError("Unknown mode: %s" % mode)

        if verbose:
            print ">> Using temporary Genomedata archive: %s" % tempdatapath

        # Load sequences if any are specified
        if seqfilenames is not None and len(seqfilenames) > 0:
            for seqfilename in seqfilenames:
                if not path(seqfilename).isfile():
                    die("Could not find sequence file: %s" % seqfilename)

            if verbose:
                print ">> Loading sequence files:"

            load_seq(tempdatapath, seqfilenames, verbose=verbose, mode=mode)

        # Load tracks if any are specified
        if tracks is not None and len(tracks) > 0:
            # Open hdf5 with track names
            try:
                track_names = []
                for track_name, track_filename in tracks:
                    if path(track_filename).isfile():
                        if track_name not in track_names:  # No duplicates
                            track_names.append(track_name)
                    else:
                        die("Could not find track file: %s" % track_filename)
            except ValueError:
                die("Error saving data from tracks: %s" % tracks)

            if verbose:
                print (">> Opening Genomedata archive with %d tracks" %
                       len(track_names))

            open_data(tempdatapath, track_names, verbose=verbose)

            # Load track data
            for track_name, track_filename in tracks:
                load_data(tempdatapath, track_name, track_filename,
                          verbose=verbose)

        # Close genomedata
        try:
            close_data(tempdatapath, verbose=verbose)
        except:
            die("Error saving metadata.")

        # Make output directory
        if verbose:
            print ">> Creating Genomedata archive: %s" % gdfilename

        # Move/repack h5 files to output directory
        if isdir:  # Repack each h5 file separately
            if gdpath.exists():
                assert gdpath.isdir()
            else:
                gdpath.makedirs()

            for tempfilepath in tempdatapath.files():
                # Repack file to output dir
                tempfilename = tempfilepath.name
                outfilepath = gdpath.joinpath(tempfilename)
                if verbose:
                    print ">> Repacking: %s -> %s" % \
                        (tempfilepath, outfilepath)

                cmd_args = ["h5repack", "-f", "GZIP=1",
                            tempfilepath, outfilepath]
                retcode = call(cmd_args)
                if retcode != 0:
                    die("HDF5 repacking failed.")
        else:  # Move the .genomedata file over
            if verbose:
                print >>sys.stderr, ">> Moving %s -> %s" % \
                    (tempdatapath, gdpath)

            tempdatapath.copyfile(gdpath)
    except:
        print >>sys.stderr, "Error creating genomedata."
        raise
    finally:
        try:
            # Remove temp directory and all contents
            if verbose:
                print ">> Cleaning up...",

            sys.stdout.flush()
            if tempdatapath.isfile():
                tempdatapath.remove()
            else:
                tempdatapath.rmtree()

            if verbose:
                print "done"
        except Exception, e:
            print >>sys.stderr, "\nCleanup failed: %s" % str(e)

    if verbose:
        print "\n===== Genomedata archive successfully created: %s =====\n" % \
            gdfilename

    return gdfilename

def parse_options(args):
    from optparse import OptionGroup, OptionParser

    usage = ("%prog [OPTIONS] GENOMEDATAFILE"
             "\ne.g. %prog -t high=signal.high.wig -t low=signal.low.bed.gz"
             " -s chrX.fa -s chrY.fa.gz mygenomedata")
    version = "%%prog %s" % __version__
    description = ("Create Genomedata archive named GENOMEDATAFILE by loading"
                   " specified track data and sequences. If GENOMEDATAFILE"
                   " already exists, it will be overwritten."
                   " --track and --sequence may be repeated to specify"
                   " multiple trackname=trackfile pairings and sequence files,"
                   " respectively.")
    parser = OptionParser(usage=usage, version=version,
                          description=description)

#     parser.add_option("-c", "--chunk-size", dest="chunk_size",
#                       metavar="NROWS", type="int",
#                       default=DEFAULT_CHUNK_SIZE,
#                       help="Chunk hdf5 data into blocks of NROWS. A higher"
#                       " value increases compression but slows random access."
#                       " Must always be smaller than the max size for a"
#                       " dataset. [default: %default]")
    group = OptionGroup(parser, "Flags")
    group.add_option("-v", "--verbose", dest="verbose",
                      default=False, action="store_true",
                      help="Print status updates and diagnostic messages")
    parser.add_option_group(group)

    group = OptionGroup(parser, "Input data")
    group.add_option("-s", "--sequence", action="append",
                      default=[],
                      help="Add the sequence data in the specified file or files"
                      " (may use UNIX glob wildcard syntax)")
    group.add_option("-t", "--track", action="append",
                      dest="track", default=[], metavar="NAME=FILE",
                      help="Add data from FILE as the track NAME,"
                      " such as: -t signal=signal.wig")
    parser.add_option_group(group)

    group = OptionGroup(parser, "Implementation")
    group.add_option("-f", "--file-mode", dest="mode",
                      default=None, action="store_const", const="file",
                      help="If specified, the Genomedata archive will be"
                      " implemented as a single file, with a separate h5 group"
                      " for each Chromosome. This is recommended if there are"
                      " a large number of Chromosomes. The default behavior is"
                      " to use a single file if there are at least %s"
                      " Chromosomes being added." % FILE_MODE_CHROMS)
    group.add_option("-d", "--directory-mode", dest="mode",
                      action="store_const", const="dir",
                      help="If specified, the Genomedata archive will be"
                      " implemented as a directory, with a separate file for"
                      " each Chromosome. This is recommended if there are a"
                      " small number of Chromosomes. The default behavior is"
                      " to use a directory if there are fewer than %s"
                      " Chromosomes being added." % FILE_MODE_CHROMS)
    parser.add_option_group(group)

    options, args = parser.parse_args(args)

    if not len(args) == 1:
        parser.error("Inappropriate number of arguments")

    return options, args

def main(args=sys.argv[1:]):
    options, args = parse_options(args)
    gdfilename = args[0]

    # list of lists
    seqfilenames_list = [glob(globname) for globname in options.sequence]
    seqfilenames = sum(seqfilenames_list, [])

    # Parse tracks into list of tuples
    try:
        tracks = []
        for track_expr in options.track:
            track_name, _, track_filename = track_expr.partition("=")
            tracks.append((track_name, track_filename))  # Tuple
    except ValueError:
        die(("Error parsing track expression: %s\nMake sure to specify tracks"
             "in NAME=FILE form, such as: -t high=signal.high") % track_expr)

    kwargs = {"verbose": options.verbose,
              "mode": options.mode}
#              "chunk_size": options.chunk_size
    load_genomedata(gdfilename, tracks, seqfilenames, **kwargs)

if __name__ == "__main__":
    sys.exit(main())
