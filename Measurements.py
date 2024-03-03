import pandas as pd
from IPython.display import display
from subprocess import run, Popen, PIPE, DEVNULL
import itertools
import io

run(["sudo", "tc", "qdisc", "add", "dev", "lo", "root", "netem", "loss", "00%"], stderr=DEVNULL)  # setup loss

loss_amounts = [0, 2, 5, 10]    # the losses for which we will run tests (in percentages)
algos = ["reno", "cubic"]       # the available TCP algorithms
port = 5000                     # the port to which the receivers will listen
runs = 20                       # the number of runs for each scenario


def set_loss(loss):     # a function to change the loss percentage
    run(["sudo", "tc", "qdisc", "change", "dev", "lo", "root", "netem", "loss", f"{loss}%"])


# test RUDP number of times provided
def test_rudp(runs):
    # open the receiver in async and send stdout to a pipe
    receiver = Popen(["./RUDP_Receiver", "-p", str(port), "-format"], stdout=PIPE, stderr=DEVNULL)
    # open the sender in sync (wait for it to finish)
    run(["./RUDP_Sender", "-p", str(port), "-ip", "127.0.0.1", "-auto", str(runs)], stdout=DEVNULL, stderr=DEVNULL)
    # pull the output of the receiver out of the pipe
    out, _ = receiver.communicate()
    # parse the output into a pandas dataframe
    df = pd.read_csv(io.StringIO(out.decode('utf-8')), index_col=0)
    print(f"Tested RUDP {runs} times")
    # display(df)
    # print the averages
    print(f"Averages: Time: {df.loc['Average', 'Time (ms)']}ms, Bandwidth: {df.loc['Average', 'Speed (MB/s)']}MB/s")
    # return the averages
    return (df.loc['Average', 'Time (ms)'], df.loc['Average', 'Speed (MB/s)'])


# test TCP number of times provided with algorithms provided
def test_tcp(runs, receiver_algo, sender_algo):
    # open the receiver in async and send stdout to a pipe
    receiver = Popen(["./TCP_Receiver", "-p", str(port), "-algo", receiver_algo, "-format"], stdout=PIPE)
    # open the sender in sync (wait for it to finish)
    run(["./TCP_Sender", "-p", str(port), "-ip", "127.0.0.1", "-algo", sender_algo, "-auto", str(runs)], stdout=DEVNULL, stderr=DEVNULL)
    # pull the output of the receiver out of the pipe
    out, _ = receiver.communicate()
    # parse the output into a pandas dataframe
    df = pd.read_csv(io.StringIO(out.decode('utf-8')), index_col=0)
    print(f"Tested TCP {runs} times, {receiver_algo=}, {sender_algo=}")
    # display(df)
    # print the averages
    print(f"Averages: Time: {df.loc['Average', 'Time (ms)']}ms, Bandwidth: {df.loc['Average', 'Speed (MB/s)']}MB/s")
    # return the averages
    return (df.loc['Average', 'Time (ms)'], df.loc['Average', 'Speed (MB/s)'])


time_df = pd.DataFrame()
bandwidth_df = pd.DataFrame()


# run through the loss amounts
for loss in loss_amounts:
    # set to the loss amount
    set_loss(loss)
    print(f"Set loss as {loss}%")
    # test UDP with the loss
    results = test_rudp(runs)
    time_df.loc['RUDP', f'{loss}%'] = results[0]
    bandwidth_df.loc['RUDP', f'{loss}%'] = results[1]
    # run through all possible combinations of receiver and sender algorithms
    for (receiver_algo, sender_algo) in itertools.product(algos, algos):
        # test TCP with the algorithms
        results = test_tcp(runs, receiver_algo, sender_algo)
        time_df.loc[f'TCP R: {receiver_algo} S: {sender_algo}', f'{loss}%'] = results[0]
        bandwidth_df.loc[f'TCP R: {receiver_algo} S: {sender_algo}', f'{loss}%'] = results[1]

# reset the loss to 0
set_loss(0)

# print the results
print()
print("Times in ms:")
display(time_df)
print()
print("Bandwidths in MB/s:")
display(bandwidth_df)
