#!/usr/bin/env python3

import os
import tempfile
import shutil
import signal

from TestHarness import Cluster, TestHelper, Utils, WalletMgr

###############################################################################
# ship_restart_test
# 
# This test verifies SHiP shuts down gracefully or recovers when restarting
# with various scenarios of corrupted log and/or index files.
# 
###############################################################################

Print=Utils.Print

args = TestHelper.parse_args({"--dump-error-details","--keep-logs","-v","--leave-running","--unshared"})

Utils.Debug=args.v
cluster=Cluster(unshared=args.unshared, keepRunning=args.leave_running, keepLogs=args.keep_logs)
dumpErrorDetails=args.dump_error_details
walletPort=TestHelper.DEFAULT_WALLET_PORT

totalProducerNodes=1
totalNonProducerNodes=1 # for SHiP node
totalNodes=totalProducerNodes+totalNonProducerNodes

walletMgr=WalletMgr(True, port=walletPort)
testSuccessful=False

prodNodeId = 0
shipNodeId = 1

origStateHistoryLog   = ""
stateHistoryLog       = ""
origStateHistoryIndex = ""
stateHistoryIndex     = ""

# Returns True if file1 and file2 contain the same content
def equalFiles(file1, file2):
    # not the same if sizes are different
    if os.path.getsize(file1) != os.path.getsize(file2):
        return False

    readSize=1024
    with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
        while True:
            bytes1 = f1.read(readSize)
            bytes2 = f2.read(readSize)

            if bytes1 != bytes2:
                return False

            # end of both files
            if not bytes1:
                return True

# Verifies that SHiP should fail to restart with a corrupted first entry header
def corruptedHeaderTest(pos, curruptedValue, shipNode):
    # restore log and index
    shutil.copyfile(origStateHistoryLog, stateHistoryLog)
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    with open(stateHistoryLog, 'rb+') as f: # opened as binary file
        f.seek(pos) # seek to the position to corrupt
        f.write(curruptedValue) # corrupt it

    isRelaunchSuccess = shipNode.relaunch()
    assert not isRelaunchSuccess, "SHiP node should have failed to relaunch"

try:
    TestHelper.printSystemInfo("BEGIN")

    cluster.setWalletMgr(walletMgr)
    Print("Stand up cluster")

    specificExtraNodeosArgs={}
    specificExtraNodeosArgs[shipNodeId]="--plugin eosio::state_history_plugin --trace-history --chain-state-history --finality-data-history --state-history-stride 200 --plugin eosio::net_api_plugin --plugin eosio::producer_api_plugin"

    if cluster.launch(topo="mesh", pnodes=totalProducerNodes, totalNodes=totalNodes,
                      activateIF=True,
                      specificExtraNodeosArgs=specificExtraNodeosArgs) is False:
        Utils.cmdError("launcher")
        Utils.errorExit("Failed to stand up cluster.")

    # Verify nodes are in sync and advancing
    cluster.waitOnClusterSync(blockAdvancing=5)
    Print("Cluster in Sync")

    Print("Shutdown unneeded bios node")
    cluster.biosNode.kill(signal.SIGTERM)

    prodNode = cluster.getNode(prodNodeId)
    shipNode = cluster.getNode(shipNodeId)

    Print("Shutdown producer and SHiP nodes")
    prodNode.kill(signal.SIGTERM)
    shipNode.kill(signal.SIGTERM)

    shipDir               = os.path.join(Utils.getNodeDataDir(shipNodeId), "state-history")
    stateHistoryLog       = os.path.join(shipDir, "chain_state_history.log")
    stateHistoryIndex     = os.path.join(shipDir, "chain_state_history.index")
    tmpDir                = tempfile.mkdtemp()
    origStateHistoryLog   = os.path.join(tmpDir, "chain_state_history.log")
    origStateHistoryIndex = os.path.join(tmpDir, "chain_state_history.index")

    # save original chain_state_history log and index files
    Print("Save original SHiP log and index")
    shutil.copyfile(stateHistoryLog, origStateHistoryLog)
    shutil.copyfile(stateHistoryIndex, origStateHistoryIndex)

    ############## Part 1: restart tests while producer node is down  #################
    
    #-------- Index file is removed. It should be regenerated at restart.
    Print("index file removed test")

    os.remove(stateHistoryIndex)

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    assert equalFiles(stateHistoryLog, origStateHistoryLog) # log unchanged
    assert equalFiles(stateHistoryIndex, origStateHistoryIndex) # index regenerated

    shipNode.kill(signal.SIGTERM) # shut down ship node for next test

    '''
    Test failure 1: index file was not regenerated. Reenable this after issue is fixed.

    #-------- Index file last entry is corrupted. It should be regenerated at restart.
    with open(stateHistoryIndex, 'rb+') as stateHistoryIndexFile: # opened as binary file
        # seek to last entry (8 bytes before the end of file)
        stateHistoryIndexFile.seek(-8, 2) # -8 for backward, 2 for starting at end

        # set the index to a random value
        stateHistoryIndexFile.write(b'\x00\x01\x02\x03\x04\x05\x06\x07')

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    assert equalFiles(stateHistoryLog, origStateHistoryLog)
    assert equalFiles(stateHistoryIndex, origStateHistoryIndex)
    '''

    #-------- Truncate index file. It should be regenerated
    #         because index size is not the same as expected size
    Print("Truncated index file test")

    # restore log and index
    shutil.copyfile(origStateHistoryLog, stateHistoryLog)
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    with open(stateHistoryIndex, 'rb+') as f:
        indexFileSize = os.path.getsize(stateHistoryIndex)
        newSize       = indexFileSize - 8
        f.truncate(newSize)

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    assert equalFiles(stateHistoryLog, origStateHistoryLog) # log file unchanged
    assert equalFiles(stateHistoryIndex, origStateHistoryIndex) # index file regenerated

    shipNode.kill(signal.SIGTERM) # shut down it for next test

    #-------- Add an extra entry to index file. It should be regenerated
    #         because index size is not the same as expected size
    Print("Extra entry index file test")

    # restore log and index
    shutil.copyfile(origStateHistoryLog, stateHistoryLog)
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    with open(stateHistoryIndex, 'rb+') as stateHistoryIndexFile: # opened as binary file
        # seek to last index, 8 bytes before the end of file
        stateHistoryIndexFile.seek(0, 2) # -8 for backward, 2 for starting at end

        # write a small value
        stateHistoryIndexFile.write(b'\x00\x00\x00\x00\x00\x00\x01\x0F')

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    assert equalFiles(stateHistoryLog, origStateHistoryLog) # log file not changed
    assert equalFiles(stateHistoryIndex, origStateHistoryIndex) # index file regenerated

    shipNode.kill(signal.SIGTERM) # shut down it for next test

    #-------- Remove log file. The log file should be reconstructed from state
    Print("Removed log file test")

    # restore index
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    os.remove(stateHistoryLog)

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"

    shipNode.kill(signal.SIGTERM) # shut down it for next test

    #-------- Corrupt first entry's magic. Relaunch should fail
    Print("first entry magic corruption test")

    corruptedHeaderTest(0, b'\x00\x01\x02\x03\x04\x05\x06\x07', shipNode) # 0 is magic's position

    #-------- Corrupt first entry's block_id. Relaunch should fail
    Print("first entry block_id corruption test")

    corruptedHeaderTest(8, b'\x00\x01\x02\x03\x04\x05\x06\x07', shipNode) # 8 is block_id's position

    '''
    # Test failure 1: Reenable this after issue is fixed.
    #-------- Corrupt first entry's payload_size. Relaunch should fail
    Print("first entry payload_size corruption test")

    corruptedHeaderTest(40, b'\x00\x00\x00\x00\x00\x00\x00\x01', shipNode) # 40 is payload_size position
    '''

    '''
    # Test failure 2: Reenable this after issue is fixed.
    #-------- Corrupt last entry's position . It should be repaired.
    # After producer node restarts, head on SHiP node should advance.
    Print("last entry postion corruption test")

    shutil.copyfile(origStateHistoryLog, stateHistoryLog)
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    with open(stateHistoryLog, 'rb+') as stateHistoryLogFile: # opened as binary file
        # seek to last index (8 bytes before the end of file)
        stateHistoryLogFile.seek(-8, 2) # -8 for backward, 2 for starting at end

        # set the index to a random value
        stateHistoryLogFile.write(b'\x00\x01\x02\x03\x04\x05\x06\x07')

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    isRelaunchSuccess = prodNode.relaunch(chainArg="--enable-stale-production")
    assert isRelaunchSuccess, "Failed to relaunch prodNode"

    assert shipNode.waitForHeadToAdvance(), "Head did not advance on shipNode"
    prodNode.kill(signal.SIGTERM)
    shipNode.kill(signal.SIGTERM)
    '''

    '''
    # Test failure 3: Reenable this after issue is fixed.
    #-------- Corrupt last entry's header. It should be repaired.
    # After producer node restarts, head on SHiP node should advance.
    Print("last entry header corruption test")

    shutil.copyfile(origStateHistoryLog, stateHistoryLog)
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    with open(stateHistoryLog, 'rb+') as f: # opened as binary file
        # seek to last index (8 bytes before the end of file)
        f.seek(-8, 2) # -8 for backward, 2 for starting at end

        data = f.read(8)
        integer_value = int.from_bytes(data, byteorder='little')
        f.seek(integer_value)

        # corrupt the header
        f.write(b'\x00\x01\x02\x03\x04\x05\x06\x07')

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    isRelaunchSuccess = prodNode.relaunch(chainArg="--enable-stale-production")
    assert isRelaunchSuccess, "Failed to relaunch prodNode"

    assert shipNode.waitForHeadToAdvance(), "Head did not advance on shipNode"
    prodNode.kill(signal.SIGTERM)
    shipNode.kill(signal.SIGTERM)
    '''

    ############## Part 2: restart tests while producer node is up  #################

    isRelaunchSuccess = prodNode.relaunch(chainArg="--enable-stale-production")
    assert isRelaunchSuccess, "Failed to relaunch prodNode"

    shutil.copyfile(origStateHistoryLog, stateHistoryLog)
    shutil.copyfile(origStateHistoryIndex, stateHistoryIndex)

    #-------- Index file is removed. It should be regenerated at restart
    os.remove(stateHistoryIndex)

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    assert shipNode.waitForHeadToAdvance(), "Head did not advance on shipNode"

    shipNode.kill(signal.SIGTERM) # shut down it for next test

    '''
    #-------- Corrupt last entry of log file. It should be repaired
    # and LIB should advance
    with open(stateHistoryLog, 'rb+') as stateHistoryLogFile: # opened as binary file
        # seek to last index, 8 bytes before the end of file
        stateHistoryLogFile.seek(-8, 2) # -8 for backward, 2 for starting at end

        # set the index to a random value
        stateHistoryLogFile.write(b'\x00\x01\x02\x03\x04\x05\x06\x07')

    isRelaunchSuccess = shipNode.relaunch()
    assert isRelaunchSuccess, "Failed to relaunch shipNode"
    assert shipNode.waitForLibToAdvance(), "LIB did not advance on shipNode"
    '''

    testSuccessful = True
finally:
    TestHelper.shutdown(cluster, walletMgr, testSuccessful=testSuccessful, dumpErrorDetails=dumpErrorDetails)
    if tmpDir is not None:
        shutil.rmtree(tmpDir, ignore_errors=True)

errorCode = 0 if testSuccessful else 1
exit(errorCode)
