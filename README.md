### NOTE:
This project has been moved to the independent Halcyon simulator project under:
* https://github.com/HalcyonGrid/whip-server

___________

# whip-server
#### The InWorldz WHIP distributed asset server

WHIPs goals are fairly straightforward. It is not intended to compete with or be a replacement for the very robust distributed databases and key/value stores available today, but it served us very well for 5 years for the storage of immutable assets in the following respects:

* Provides cheap RF=1 storage once a peer server has been filled
* Allows for a primary with replica nodes to protect the active writeable server(s) from data loss
* Provides the ability to easily add more servers to serve as storage nodes enabling scale-out storage for a large volume of assets
* Storage design provides quick, file based backup and restore in the case of a node failure. Asset space is partitioned into a manageable number of files.
* Provides built in caching for quick reads on nodes with a lot of RAM
* Allows for the easy pruning of temporary assets used for avatar appearance without shuffling them between simulators
* Allows for online adding of new nodes to increase total storage space.

WHIP has been extensively tested on windows with very little runtime on linux.
There may be bugs in the posix implementation that need fixing.

#### Things WHIP doesn't do
* WHIP is a storage layer and doesn't include any deduplication by default.

#### Things I'd love to see implemented
* Native deduplication without an extra layer
* Implementation of bloom filters and filter sharing between read-only nodes to increase the efficiency of queries on a large number of nodes
