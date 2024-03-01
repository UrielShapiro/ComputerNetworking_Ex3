import pandas as pd
from IPython.display import display
from subprocess import run, Popen, PIPE, DEVNULL
import itertools
import io

run(["sudo", "tc", "qdisc", "add", "dev", "lo", "root", "netem", "loss", "00%"], stderr=DEVNULL)  # setup loss

loss_amounts = [0, 2, 5, 10]
algos = ["reno", "cubic"]
port = 5000
runs = 5


def set_loss(loss):
    run(["sudo", "tc", "qdisc", "change", "dev", "lo", "root", "netem", "loss", f"{loss}%"])


def test_rudp(runs):
    receiver = Popen(["./RUDP_Receiver", "-p", str(port), "-format"], stdout=PIPE, stderr=DEVNULL)
    run(["./RUDP_Sender", "-p", str(port), "-ip", "127.0.0.1", "-auto", str(runs)], stdout=DEVNULL, stderr=DEVNULL)
    out, _ = receiver.communicate()
    df = pd.read_csv(io.StringIO(out.decode('utf-8')), index_col=0)
    print(f"Tested RUDP {runs} times")
    # display(df)
    print(f"Average: {df.loc['Average', 'Time (ms)']}")
    if len(df) != runs+1:
        print(f"PROBLEM!!!!!: {runs+1 - len(df)}")


def test_tcp(runs, receiver_algo, sender_algo):
    receiver = Popen(["./TCP_Receiver", "-p", str(port), "-algo", receiver_algo, "-format"], stdout=PIPE)
    run(["./TCP_Sender", "-p", str(port), "-ip", "127.0.0.1", "-algo", sender_algo, "-auto", str(runs)], stdout=DEVNULL, stderr=DEVNULL)
    out, _ = receiver.communicate()
    df = pd.read_csv(io.StringIO(out.decode('utf-8')), index_col=0)
    print(f"Tested TCP {runs} times, {receiver_algo=}, {sender_algo=}")
    # display(df)
    print(f"Average: {df.loc['Average', 'Time (ms)']}")
    if len(df) != runs+1:
        print(f"PROBLEM!!!!!: {runs+1 - len(df)}")


for loss in loss_amounts:
    set_loss(loss)
    print(f"Set loss as {loss}%")
    test_rudp(runs)
    for (receiver_algo, sender_algo) in itertools.product(algos, algos):
        test_tcp(runs, receiver_algo, sender_algo)

set_loss(0)
