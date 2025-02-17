#!/usr/bin/env python3
import random
import string
import subprocess
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from io import StringIO
from matplotlib import cm


def RunIteration(mappings=10000, cells=20):
    pres = subprocess.run(["../build/demo/demo",
                           "--mappings", str(mappings),
                           "--cells", str(cells),
                           "maxerror"],
                          capture_output=True,
                          text=True,
                          check=True)
    return pres.stdout


def XCellsYMappingsMaxError(min_mappings=1,
                            min_cells=1,
                            max_mappings=200,
                            max_cells=20,
                            step_mappings=10,
                            step_cells=1):
    x = []
    y = []
    z = []
    count = 0
    for mappings in range(min_mappings, max_mappings, step_mappings):
        for cells in range(min_cells, max_cells, step_cells):
            x.append(mappings)
            y.append(cells)
            z.append(float(RunIteration(mappings, cells)))
            count += 1
            if count % 100 == 0:
                print(count, "\n")
    return [x, y, z]


def PlotXCellsYMappingsMaxError():
    dots = XCellsYMappingsMaxError()
    X = np.array(dots[0])
    Y = np.array(dots[1])
    Z = np.array(dots[2])

    fig, ax = plt.subplots(subplot_kw={"projection": "3d"})
    surf = ax.plot_trisurf(X, Y, Z, cmap=cm.coolwarm, vmin=Z.min(),
                           vmax=Z.max(),
                           linewidth=1, antialiased=False)
    ax.set_xlabel("Mappings")
    ax.set_ylabel("Cells")
    # with plt.xkcd():
    #    plt.plot(time, amplitude)
    #    plt.title('Sine wave')
    #    plt.xlabel('Time')
    #    plt.ylabel('Amplitude = sin(time)')
    #    plt.axhline(y = 0, color ='k')

    plt.show()


def GenReal():
    letters = string.ascii_lowercase
    return ''.join(random.choice(letters) for i in range(8))


def Plot1100MaxError():
    res = []
    cfg = "weak 1 1\nstrong 2 100\n"
    for i in range(3, 100):
        print(i)
        print(cfg)
        cfg += f"{GenReal()} {i} 100\n"
        res.append(subprocess.run(["../build/demo/demo",
                                   "--mappings", str(10000),
                                   "--cells", str(20),
                                   "--stdin",
                                   "maxerror"],
                                  input=cfg,
                                  capture_output=True, text=True, check=True).stdout)
    plt.plot(res)
    plt.show()


# Plot1100MaxError()
# PlotXCellsYMappingsMaxError()


def PlotAbsoluteDifferenceUniformity():
    pres = subprocess.run(["../build/demo/demo",
                           "--mappings", "100",
                           "--cells", "20",
                           "yielduniabs"],
                          capture_output=True,
                          text=True,
                          check=True)
    id = []
    iter = []
    cnt = []
    for line in pres.stdout.splitlines():
        words = line.split(sep=';')
        iter.append(words[0])
        id.append(words[1])
        cnt.append(words[2])

    fig, ax = plt.subplots(subplot_kw={"projection": "3d"})
    surf = ax.plot_trisurf(np.array(id, dtype='float64'), np.array(iter, dtype='float64'), np.array(cnt, dtype='float64'), cmap=cm.coolwarm,
                           linewidth=1, antialiased=False)
    # sc = ax.scatter(np.array(id, dtype='float64'), np.array(iter, dtype='float64'), np.array(cnt, dtype='float64'))
    fig.set_figwidth(5)
    fig.set_figheight(5)
    ax.set_xlabel("Ids")
    ax.set_ylabel("Disabled")
    plt.show()


def PlotAbsoluteDifferenceUniformityMax():
    pres = subprocess.run(["../build/demo/demo",
                           "--mappings", "100",
                           "--cells", "20",
                           "maxyielduniabs"],
                          capture_output=True,
                          text=True,
                          check=True)
    mdiff = []
    fig, ax = plt.subplots(figsize=(16, 9))
    for line in pres.stdout.splitlines():
        mdiff.append(float(line))
    ax.grid()
    ax.set_title("Maximum weight added as fraction of own fair weight")
    ax.set_xlabel("Disabled count")
    ax.set_ylabel("Maximum")
    ax.set_xlim(left=0)
    ax.plot(mdiff)
    ax.plot([0, len(mdiff)], [0, 1], marker='o', linestyle='--', color='green')
    # ax.plot(np.array(mdiff, dtype='float64'))
    # print(mdiff)
    plt.show()


def PlotMissing():
    pres = subprocess.run(["../build/demo/demo",
                           "--mappings", "100",
                           "--cells", "20",
                           "missing"],
                          capture_output=True,
                          text=True,
                          check=True)
    # df = pd.read_csv("missing", sep=";")
    df = pd.read_csv(StringIO(pres.stdout), sep=";")
    x = df["disfrac"].values.tolist()
    y = df["similarity"].values.tolist()
    fig, ax = plt.subplots(figsize=(16, 9))
    ax.set_title("Effect of disabling keys on lookup consistency")
    ax.set_xlabel("Disabled fraction")
    ax.set_ylabel("Lookup similarity to all enabled")
    ax.plot(x, y)
    plt.show()


# PlotAbsoluteDifferenceUniformity()
# PlotAbsoluteDifferenceUniformityMax()
PlotMissing()
