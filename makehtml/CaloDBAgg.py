#!/usr/bin/env python

import os
import pyodbc
import subprocess
import glob
import time
import argparse
import hashlib

track_hist_types = ["HIST_CALOQA", "HIST_CALOFITTINGQA","HIST_JETS"]
runtypes = ["run3auau"]


parser = argparse.ArgumentParser(description="Aggregate the QA histogram files produced for each DST segment of a run into a single QA histogram file per run.")
parser.add_argument("-v","--verbose",help="add additional printing", action="store_true")
parser.add_argument("-t","--test",help="run a verbose test without actually aggregating", action="store_true")
args = parser.parse_args()
if args.test and not args.verbose:
    args.verbose = True
print("Verbose is " + str(args.verbose))
print("Test is " + str(args.test))


def get_unique_run_dataset_pairs(cursor, type, runtype):
    dsttype = type
    query = "SELECT runnumber, tag FROM datasets WHERE dsttype='{}' GROUP BY runnumber,tag order by runnumber desc;".format(dsttype)
    if args.verbose:
        print(query)
    cursor.execute(query)
    runnumbers = {(row.runnumber, row.tag) for row in cursor.fetchall()}
    
    return runnumbers

def getPaths(cursor, run, dataset, type, runtype):
    query = "SELECT files.full_file_path FROM files,datasets WHERE datasets.runnumber={} AND datasets.dataset='{}' AND datasets.dsttype='{}' AND datasets.tag='{}' AND files.lfn=datasets.filename".format(run,runtype,type,dataset)
    if args.verbose == True:
        print(query)
    cursor.execute(query)
    filepaths = {(row.full_file_path) for row in cursor.fetchall()}
    return filepaths

def getBuildDbTag(type, filename):
    parts = filename.split(os.sep)
    index = parts.index(type)
    
    return parts[index+3]

def main():   
    import time
    conn = pyodbc.connect("DSN=FileCatalog_read;UID=phnxrc;READONLY=True")
    cursor = conn.cursor()
    FCWrite = pyodbc.connect("DSN=FileCatalog;UID=phnxrc")
    FCWritecursor = FCWrite.cursor()
    aggDirectory = "/sphenix/data/data02/sphnxpro/QAhtml/aggregated/"
    for runtype in runtypes:
        for histtype in track_hist_types:
            runs_dbtags = get_unique_run_dataset_pairs(cursor, histtype, runtype)
            for run, dbtag in runs_dbtags:
                print("Processing run " + str(run))
                if run < 66000:
                    continue
                filepaths = getPaths(cursor, run, dbtag, histtype, runtype)
                if args.verbose == True:
                    print("all total filepaths")
                    print(filepaths)
                if(len(filepaths) == 0):
                    continue
                tags = next(iter(filepaths)).split(os.sep)
                
                index = tags.index(runtype)
                
                collisiontag = tags[index]
                beamtag = tags[index+1]
                anadbtag = dbtag
                dsttypetag = tags[index+2]
                rundirtag = tags[index+4]
                
                # make an analogous path to the production DST in sphenix/data
                completeAggDir = aggDirectory + collisiontag + "/" + beamtag + "/" + anadbtag + "/" + dsttypetag + "/" + rundirtag + "/"
                if args.verbose == True:
                    print("aggregated directory")
                    print(completeAggDir)
                    
                filepathWildcard = completeAggDir + histtype + "*" + dbtag + "*" + str(run) + "*"

                if not os.path.isdir(completeAggDir):
                    if args.verbose == True:
                        print("making a new aggregated dir")
                    if not args.test == True:
                        os.makedirs(completeAggDir, exist_ok=True)

                
                filepath = glob.glob(filepathWildcard)

                path = ""
                aggFileTime = 0
                if len(filepath) > 0:
                    path = filepath[0]
                    aggFileTime = os.path.getmtime(path)

                #need to figure out the latest db tag
                latestdbtag= ""
                latestdbtagInt=0
                for newpath in filepaths:
                    if not os.path.exists(newpath):
                        continue
                    thistag = getBuildDbTag(runtype,newpath)
                    tags = thistag.split("_")
                   
                    if tags[1].find("cdbtag") != -1:
                        latestdbtag=thistag
                        break
                    if int(tags[1].split("p")[1]) > latestdbtagInt:
                        latestdbtag=thistag
                        latestdbtagInt = int(tags[1].split("p")[1])

                reagg=False
                if len(path) == 0:
                    reagg=True
                newFileTime = 0
                if reagg == False:
                    for newpath in filepaths:
                        if newpath.find(latestdbtag) == -1:
                            continue
                        if os.path.getmtime(newpath) > newFileTime:
                            newFileTime = os.path.getmtime(newpath)
                        if os.path.getmtime(newpath) > aggFileTime:
                            reagg = True
                            break
                if args.verbose == True:
                    print("Agg file " + path + "  time is " + str(aggFileTime))
                    print("latest new file time is " + str(newFileTime))
                if reagg == False:
                    print("Don't need to reagg " + path)
                    continue
                filestoadd = []
                nfiles = 0
                lfn = histtype + "_" + runtype + "_" + dbtag + "-{:08d}-9999.root".format(run)
                path = completeAggDir + lfn

                if args.verbose == True:
                    print("agg file path is " + path)
                command = ["hadd","-ff",path]
                for newpath in filepaths:
                    # make sure the file has the same db tag
                    if newpath.find(latestdbtag) == -1: 
                       continue
                    command.append(str(newpath))
                    nfiles+=1

                # wait for at least 4 files
                if nfiles < 4:
                    print("not enough files " + str(run))
                    continue
                if args.verbose:
                    print("executing command")
                    print(command)
                if not args.test:
                    subprocess.run(command)

                    #insert into filecatalog
                    size= int( os.path.getsize(path) )
                    file_hash=None
                    with open( path, "rb") as f:
                        file_hash = hashlib.md5()
                        chunk = f.read(8192)
                        while chunk:
                            file_hash.update(chunk)
                            chunk = f.read(8192)
                    md5 = file_hash.hexdigest()
                    
                    insertquery="""
                    insert into files (lfn,full_host_name,full_file_path,time,size,md5) 
                    values ('{}','gpfs','{}','now',{},'{}')
                    on conflict
                    on constraint files_pkey
                    do update set 
                    time=EXCLUDED.time,
                    size=EXCLUDED.size,
                    md5=EXCLUDED.md5
                    ;
                    """.format(lfn,path,size,md5)
                    if args.verbose :
                        print(insertquery)
                    FCWritecursor.execute(insertquery)
                    FCWritecursor.commit()

                    insertquery="""
                    insert into datasets (filename,runnumber,segment,size,dataset,dsttype)
                    values ('{}','{}',9999,'{}','{}','{}')
                    on conflict
                    on constraint datasets_pkey
                    do update set
                    runnumber=EXCLUDED.runnumber,
                    segment=EXCLUDED.segment,
                    size=EXCLUDED.size,
                    dsttype=EXCLUDED.dsttype,
                    events=EXCLUDED.events
                    ;
                    """.format(lfn,run,size,dbtag,histtype)
                    if args.verbose :
                        print(insertquery)
                    FCWritecursor.execute(insertquery)
                    FCWritecursor.commit()
                    
    conn.close()
    FCWrite.close()
if __name__ == "__main__":
    main()
