#!/usr/bin/env python3
import random
import string
import subprocess
import numpy as np
import matplotlib.pyplot as plt
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
    for i in range(3,100):
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
PlotXCellsYMappingsMaxError()
