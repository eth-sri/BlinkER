// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Body_h
#define Body_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/streams/ReadableStreamImpl.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class BodyStreamBuffer;
class ScriptState;

class Body
    : public GarbageCollectedFinalized<Body>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public FileReaderLoaderClient {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Body);
public:
    explicit Body(ExecutionContext*);
    virtual ~Body() { }
    enum ResponseType {
        ResponseUnknown,
        ResponseAsArrayBuffer,
        ResponseAsBlob,
        ResponseAsFormData,
        ResponseAsJSON,
        ResponseAsText
    };

    ScriptPromise arrayBuffer(ScriptState*);
    ScriptPromise blob(ScriptState*);
    ScriptPromise formData(ScriptState*);
    ScriptPromise json(ScriptState*);
    ScriptPromise text(ScriptState*);
    ReadableStream* body();

    // Sets the bodyUsed flag to true. This signifies that the contents of the
    // body have been consumed and cannot be accessed again.
    void setBodyUsed();
    bool bodyUsed() const;

    bool streamAccessed() const;

    // ActiveDOMObject override.
    virtual void stop() override;
    virtual bool hasPendingActivity() const override;

    virtual void trace(Visitor*) override;

protected:
    // Copy constructor for clone() implementations
    explicit Body(const Body&);

private:
    class ReadableStreamSource;
    class BlobHandleReceiver;

    void pullSource();
    void readAllFromStream(ScriptState*);
    ScriptPromise readAsync(ScriptState*, ResponseType);
    void readAsyncFromBlob(PassRefPtr<BlobDataHandle>);
    void resolveJSON(const String&);

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() override;
    virtual void didReceiveData() override;
    virtual void didFinishLoading() override;
    virtual void didFail(FileError::ErrorCode) override;

    void didBlobHandleReceiveError(PassRefPtrWillBeRawPtr<DOMException>);

    // We use BlobDataHandle or BodyStreamBuffer as data container of the Body.
    // BodyStreamBuffer is used only when the Response object is created by
    // fetch() API.
    // FIXME: We should seek a cleaner way to handle the data.
    virtual PassRefPtr<BlobDataHandle> blobDataHandle() const = 0;
    virtual BodyStreamBuffer* buffer() const = 0;
    virtual String contentTypeForBuffer() const = 0;

    void didFinishLoadingViaStream(DOMArrayBuffer*);

    OwnPtr<FileReaderLoader> m_loader;
    bool m_bodyUsed;
    bool m_streamAccessed;
    ResponseType m_responseType;
    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
    Member<ReadableStreamSource> m_streamSource;
    Member<ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>> m_stream;
};

} // namespace blink

#endif // Body_h
