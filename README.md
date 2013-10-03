
Flv Media Source
=========

Windows Media Foundation Flv MediaSource

----------------------------------------

#### Windows Media Foundation

这方面资料比较少，除了微软提供的MPEG1Source 和 WavSource，互联网上基本找不到其他的Media Source实现。读懂MPEG1Source非常重要。

网上有基于Silverlight的 Flv MediaElementSource实现，也是独此一份。关于Flv的实现细节，可以参考这个。

实现一个MediaSource需要具备以下基础知识

- In process `COM`

    WMF 基于COM。MPEG1Source和WavSource都是徒手实现的COM组件。用COM的术语来说就是：进程内，自由线程的COM对象。这个时候你得为每个对象实现IUnknown的三个函数，小心维持每个COM对象的引用计数。无聊而又无奈好在进程内。
    
- Async Call
 
    无可避免，MediaSource是异步工作的，所以确保MediaSource线程安全是必须的。MediaSource和MediaStream相互引用，MediaStream也是线程安全的，我们得避免MediaStream和MediaSource出现互锁的情况。MPEG1Source是通过MediaSource和MediaStream共享同一个临界区的方式解决这个问题的。简单而且有效。

- 令牌桶式帧队列。MF Player向MediaSource拉音视频帧数据，Player向每个活动的MediaStream发令牌请求数据帧，另一方面，Media Stream接受Media Source分离出来的数据帧。Media Stream获得数据帧后，连同令牌通过MediaEvent 发送到Player。
- WMF的异步调用模型
    - IAsyncCallback / IAsyncResult
    - IMFEventGenerator
    - MFPutWorkItem / Worker Queue
    - Beginxx / Endxx
- Media Source的状态和状态转移

    Player通过/HKCU/Software/Microsoft/Media Foundation/Byte Stream Handler/.flv找到能够处理.flv文件的FlvByteStreamHandler。这是FlvSource.dll唯一需要类厂的COM对象。ByteStreamHandler在MediaSource确定MediaSubType及需要的参数后，通知Player Media Source已经可用

    Player向MediaStream发出数据帧令牌，MediaStream将请求转给MediaSource。MediaSource在收到请求后，分离各流的帧数据，并将这些帧分发到流上。media stream再通过media event的方式送到解码器。
    
    Media Source至少有这些状态
    - UnOpened    : 起始状态
    - Openning    : 还不能确定MediaType
    - Running   :   已经根据MediaType，创建合适的PresentationDescription
    - Pausing
    - Stopped
    - Shutdown
    
#### Flv Encoded via H264 and AAC or MP3
#### WRL Based
