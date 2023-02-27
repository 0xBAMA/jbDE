# import matplotlib
# non_gui_backends = matplotlib.rcsetup.non_interactive_bk
# print ("Non Gui backends are:", non_gui_backends)

# plt.switch_backend('svg')
# import matplotlib.pyplot as plt

import matplotlib
matplotlib.use('TkCairo')

import numpy as np
import matplotlib.pyplot as plt

cmaps = [('_uniform', [
			'viridis', 'plasma', 'inferno', 'magma', 'cividis']),
		('_sequential', [
			'Greys', 'Purples', 'Blues', 'Greens', 'Oranges', 'Reds',
			'YlOrBr', 'YlOrRd', 'OrRd', 'PuRd', 'RdPu', 'BuPu',
			'GnBu', 'PuBu', 'YlGnBu', 'PuBuGn', 'BuGn', 'YlGn',
			'binary', 'gist_yarg', 'gist_gray', 'gray', 'bone', 'pink',
			'spring', 'summer', 'autumn', 'winter', 'cool', 'Wistia',
			'hot', 'afmhot', 'gist_heat', 'copper']),
		('_diverging', [
			'PiYG', 'PRGn', 'BrBG', 'PuOr', 'RdGy', 'RdBu',
			'RdYlBu', 'RdYlGn', 'Spectral', 'coolwarm', 'bwr', 'seismic']),
		('_cyc', ['twilight', 'twilight_shifted', 'hsv']),
		('_qualitative', [
			'Pastel1', 'Pastel2', 'Paired', 'Accent',
			'Dark2', 'Set1', 'Set2', 'Set3',
			'tab10', 'tab20', 'tab20b', 'tab20c']),
		('_misc', [
			'flag', 'prism', 'ocean', 'gist_earth', 'terrain', 'gist_stern',
			'gnuplot', 'gnuplot2', 'CMRmap', 'cubehelix', 'brg',
			'gist_rainbow', 'rainbow', 'jet', 'turbo', 'nipy_spectral',
			'gist_ncar'])]

gradient = np.linspace(0, 1, 256)
gradient = np.vstack((gradient, gradient))

from pylab import *

def plot_color_gradients(cmap_category, cmap_list):
	# Create figure and adjust figure height to number of colormaps
	# nrows = len(cmap_list)
	# figh = 0.35 + 0.15 + ( nrows + ( nrows - 1 ) * 0.1 ) * 0.22
	# fig, axs = plt.subplots(nrows=nrows, figsize=(3.4, figh))
	# fig.subplots_adjust(top=1-.35/figh, bottom=.15/figh, left=0.2, right=0.99)

	# axs[0].set_title(f"{cmap_category} colormaps", fontsize=14)

	for cmap_name in cmap_list:
		# ax.imshow(gradient, aspect='auto', cmap=cmap_name)
		title = "matplotlib" + cmap_category + "_" + cmap_name
		f = open( title + ".hex", "w" )
		print( title )
		cmap = cm.get_cmap( cmap_name, 256)  # PiYG
		for i in range(cmap.N):
			rgba = cmap(i)
			f.write(matplotlib.colors.rgb2hex(rgba) + '\n') # rgb2hex accepts rgb or rgba
		# ax.text(-.01, .5, title, va='center', ha='right', fontsize=10, transform=ax.transAxes)

	# # Turn off *all* ticks & spines, not just the ones with colormaps.
	# for ax in axs:
	# 	ax.set_axis_off()

	# plt.savefig( cmap_category, bbox_inches='tight' )


for cmap_category, cmap_list in cmaps:
	plot_color_gradients(cmap_category, cmap_list)


# - - - - - - - - -  - - - - - - - - -

