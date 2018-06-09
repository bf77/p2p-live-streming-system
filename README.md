# P2P live streaming system


- WebM Format Structure

WebM Structure | Discription
--- | ---
*Header* | EBML<br>Segment<br>├─Void<br>├─Info<br>├─Tracks
*Cluster* | Assembled SimpleBlock
*SimpleBlock* | Movie Data Body<br>

- Cluster Structure

Cluster Format | Contents
--- | ---
*Cluster Header* | `0x1f 0x43 0xb6 0x75 0x01 0xXX 0xXX 0xXX 0xXX 0xXX 0xXX 0xXX`
*Data* | `0xXX ... ... ... ...`
*Footer* | `0x1f 0x43 0xb6 0x75 0x01 0xff 0xff 0xff 0xff 0xff 0xff 0xff`

***\*Cluster Header (including bytes to indicate the total data )***


### Reference
***

* [UPnP](http://hp.vector.co.jp/authors/VA011804/upnp.htm#UPNP_020)
* [Description:SSDP](http://d.hatena.ne.jp/ozakira/20141130)
* [Usage:UPnP](http://d.hatena.ne.jp/yogit/20101006/1286380061)
* [Implementation:torrent](https://www.gitbook.com/book/kyorohiro/doc_hetimatorrent/details)
* [Matroska Specifications](https://www.matroska.org/technical/specs/index.html)
* [Writing Samples for Structure](http://docs.ros.org/diamondback/api/artoolkit/html/structARParam.html)
* [Markdown Cheat Sheet for Github](https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet)
