FlvSource
=========

Windows Media Foundation Flv Media Source

-----------------

#### Windows Media Foundation Media Source Implementation
互联网上关于WMF的资源还比较少，好像这玩意儿除了微软，别人都完全不感兴趣。想要学习它需要一番功夫，好比起DShow来，WMF使用和理解起来都要简单很多。

如何WPF应用中在线播放FLV/F4V/MP4流是最原始的需求。为什么是WPF? 我也不知道。桌面UI开发远比想象的困难, 或许HTML5在桌面开发中会大行其道，但是现在我决定选择WPF。

WPF应用内嵌的MediaElement没有什么开发模型，甚至不如Silverlight。如果想在在WPF应用中播放FLV, 我们首先得实现一个Flv MediaSource，其次还得实现一个Progressive Downloader。这个downloader现在我们不讨论，因为通过HTTP Range实现这么一个东西还是有可能的。

##### 实现Media Source 都要解决什么问题
- WMF是基于COM的，所以你得实现关于COM的一堆东西


#### Based on `WRL` Windows Runtime Libary C++ Template
#### Works with Flv File encoded by avc and aac or mp3
#### Pure C++ implementation
#### Used some c++11 features
#### Just a demo


    [1]: https://hg.codeplex.com/mediastreamsources