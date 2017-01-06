import pexpect
import sys
from termcolor import cprint

debug=True

def info(msg):
    cprint('--> ' + msg, 'green')

def spawnQemuSession():
    info('Starting QEMU...')
    session = pexpect.spawn('qemu-system-arm -M vexpress-a9 -m 1024M -kernel images/u-boot -drive file=images/vexpress.img,if=sd,format=raw -net nic,model=lan9118 -net user,hostfwd=tcp::10022-:22 -nographic')
    if debug:
        session.logfile = sys.stdout
    info('Waiting for Linux to boot...')
    session.expect('Starting dropbear sshd')
    return session

def runLogin(session):
    info('Logging in...')
    session.expect('login: ')
    session.sendline('root')
    session.expect('Password:')
    session.sendline('root')

def poweroff(session):
    info('Powering off...')
    session.expect('#')
    session.sendline('poweroff')
    session.expect(pexpect.EOF)
    session.close

def reboot(session):
    info('Rebooting...')
    session.expect('#')
    session.sendline('reboot')

### Tests
def testImageWorks():
    session = spawnQemuSession()
    runLogin(session)
    poweroff(session)

def scpFileToQemu(hostFilename, targetFilename):
    info('Copying {0} to target...'.format(hostFilename))
    scp = pexpect.spawn('scp -P 10022 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o PreferredAuthentications=password {0} root@localhost:{1}'.format(hostFilename, targetFilename))
    scp.expect('password:')
    scp.sendline('root')
    scp.expect(pexpect.EOF)
    scp.close


def firmwareUpdate():
    session = spawnQemuSession()
    runLogin(session)
    session.expect('#')
    session.sendline('rm /mnt/*.fw')
    session.expect('#')
    session.sendline('ls -las /mnt')
    scpFileToQemu('images/vexpress.fw', '/mnt')
    session.expect('#')
    session.sendline('ls -las /mnt')
    poweroff(session)



firmwareUpdate()
