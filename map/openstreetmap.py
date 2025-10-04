import osmnx as ox
import networkx as nx
import matplotlib.pyplot as plt

place_name = "London, UK" #"Gdansk, Poland" # "Manhattan, New York, USA"

G = ox.graph_from_place(place_name, network_type='drive')  # 'drive' = drivable roads

tags = {'waterway': 'river'}
rivers = ox.features_from_place(place_name, tags=tags)

fig, ax = ox.plot_graph(G, node_size=0, edge_color='blue', edge_linewidth=1)

rivers.plot(ax=ax, color='cyan', linewidth=2)
plt.show()

nodes, edges = ox.graph_to_gdfs(G)

street_names = edges['name'].unique()
print(street_names)

ox.save_graphml(G, filepath="manhattan.graphml")

edges.to_file("manhattan_edges.shp")