/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLDocumentParser_h
#define HTMLDocumentParser_h

#include "core/dom/ParserContentPolicy.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/eventracer/EventRacerJoinActions.h"
#include "core/fetch/ResourceClient.h"
#include "core/frame/UseCounter.h"
#include "core/html/parser/BackgroundHTMLInputStream.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLInputStream.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLPreloadScanner.h"
#include "core/html/parser/HTMLScriptRunnerHost.h"
#include "core/html/parser/HTMLSourceTracker.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "core/html/parser/HTMLTreeBuilderSimulator.h"
#include "core/html/parser/ParserSynchronizationPolicy.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/html/parser/XSSAuditor.h"
#include "core/html/parser/XSSAuditorDelegate.h"
#include "platform/text/SegmentedString.h"
#include "wtf/Deque.h"
#include "wtf/OwnPtr.h"
#include "wtf/WeakPtr.h"
#include "wtf/text/TextPosition.h"

namespace blink {

class BackgroundHTMLParser;
class CompactHTMLToken;
class Document;
class DocumentEncodingData;
class DocumentFragment;
class EventRacerLog;
class EventAction;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLScriptRunner;
class HTMLTreeBuilder;
class HTMLResourcePreloader;

class PumpSession;

class HTMLDocumentParser :  public ScriptableDocumentParser, private HTMLScriptRunnerHost {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLDocumentParser);
public:
    static PassRefPtrWillBeRawPtr<HTMLDocumentParser> create(HTMLDocument& document, bool reportErrors, ParserSynchronizationPolicy backgroundParsingPolicy)
    {
        return adoptRefWillBeNoop(new HTMLDocumentParser(document, reportErrors, backgroundParsingPolicy));
    }
    virtual ~HTMLDocumentParser();
    virtual void trace(Visitor*) override;

    // Exposed for HTMLParserScheduler
    void resumeParsingAfterYield();

    static void parseDocumentFragment(const String&, DocumentFragment*, Element* contextElement, ParserContentPolicy = AllowScriptingContent);

    HTMLTokenizer* tokenizer() const { return m_tokenizer.get(); }

    virtual TextPosition textPosition() const override final;
    virtual bool isParsingAtLineNumber() const override final;
    virtual OrdinalNumber lineNumber() const override final;

    virtual void suspendScheduledTasks() override final;
    virtual void resumeScheduledTasks() override final;

    struct ParsedChunk {
        OwnPtr<CompactHTMLTokenStream> tokens;
        PreloadRequestStream preloads;
        XSSInfoStream xssInfos;
        HTMLTokenizer::State tokenizerState;
        HTMLTreeBuilderSimulator::State treeBuilderState;
        HTMLInputCheckpoint inputCheckpoint;
        TokenPreloadScannerCheckpoint preloadScannerCheckpoint;
        bool startingScript;
    };
    void didReceiveParsedChunkFromBackgroundParser(PassOwnPtr<ParsedChunk>);
    void didReceiveEncodingDataFromBackgroundParser(const DocumentEncodingData&);

    virtual void appendBytes(const char* bytes, size_t length) override;
    virtual void flush() override final;
    virtual void setDecoder(PassOwnPtr<TextResourceDecoder>) override final;

    UseCounter* useCounter() { return UseCounter::getFrom(contextForParsingSession()); }

protected:
    virtual void insert(const SegmentedString&) override final;
    virtual void append(const String&) override;
    virtual void finish() override final;

    HTMLDocumentParser(HTMLDocument&, bool reportErrors, ParserSynchronizationPolicy);
    HTMLDocumentParser(DocumentFragment*, Element* contextElement, ParserContentPolicy);

    HTMLTreeBuilder* treeBuilder() const { return m_treeBuilder.get(); }

    void forcePlaintextForTextDocument();

private:
    static PassRefPtrWillBeRawPtr<HTMLDocumentParser> create(DocumentFragment* fragment, Element* contextElement, ParserContentPolicy parserContentPolicy)
    {
        return adoptRefWillBeNoop(new HTMLDocumentParser(fragment, contextElement, parserContentPolicy));
    }

    // DocumentParser
    virtual void detach() override final;
    virtual bool hasInsertionPoint() override final;
    virtual bool processingData() const override final;
    virtual void prepareToStopParsing() override final;
    virtual void stopParsing() override final;
    virtual bool isWaitingForScripts() const override final;
    virtual bool isExecutingScript() const override final;
    virtual void executeScriptsWaitingForResources() override final;
    void doExecuteScriptsWaitingForResources();

    // HTMLScriptRunnerHost
    virtual void notifyScriptLoaded(Resource*) override final;
    void doNotifyScriptLoaded(Resource*);
    virtual HTMLInputStream& inputStream() override final { return m_input; }
    virtual bool hasPreloadScanner() const override final { return m_preloadScanner.get() && !shouldUseThreading(); }
    virtual void appendCurrentInputStreamToPreloadScannerAndScan() override final;

    void startBackgroundParser();
    void stopBackgroundParser();
    void validateSpeculations(PassOwnPtr<ParsedChunk> lastChunk);
    void discardSpeculationsAndResumeFrom(PassOwnPtr<ParsedChunk> lastChunk, PassOwnPtr<HTMLToken>, PassOwnPtr<HTMLTokenizer>);
    size_t processParsedChunkFromBackgroundParser(PassOwnPtr<ParsedChunk>);
    void pumpPendingSpeculations();

    Document* contextForParsingSession();

    bool canTakeNextToken();
    void pumpTokenizer();
    void pumpTokenizerIfPossible();
    void constructTreeFromHTMLToken();
    void constructTreeFromCompactHTMLToken(const CompactHTMLToken&);

    void runScriptsForPausedTreeBuilder();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();
    void attemptToRunDeferredScriptsAndEnd();
    void end();

    bool shouldUseThreading() const { return m_shouldUseThreading; }

    bool isParsingFragment() const;
    bool isScheduledForResume() const;
    bool inPumpSession() const { return m_pumpSessionNestingLevel > 0; }
    bool shouldDelayEnd() const { return inPumpSession() || isWaitingForScripts() || isScheduledForResume() || isExecutingScript(); }

    HTMLToken& token() { return *m_token; }

    HTMLParserOptions m_options;
    HTMLInputStream m_input;

    OwnPtr<HTMLToken> m_token;
    OwnPtr<HTMLTokenizer> m_tokenizer;
    OwnPtrWillBeMember<HTMLScriptRunner> m_scriptRunner;
    OwnPtrWillBeMember<HTMLTreeBuilder> m_treeBuilder;
    OwnPtr<HTMLPreloadScanner> m_preloadScanner;
    OwnPtr<HTMLPreloadScanner> m_insertionPreloadScanner;
    OwnPtr<HTMLParserScheduler> m_parserScheduler;
    HTMLSourceTracker m_sourceTracker;
    TextPosition m_textPosition;
    XSSAuditor m_xssAuditor;
    XSSAuditorDelegate m_xssAuditorDelegate;

    // FIXME: m_lastChunkBeforeScript, m_tokenizer, m_token, and m_input should be combined into a single state object
    // so they can be set and cleared together and passed between threads together.
    OwnPtr<ParsedChunk> m_lastChunkBeforeScript;
    Deque<OwnPtr<ParsedChunk>> m_speculations;
    WeakPtrFactory<HTMLDocumentParser> m_weakFactory;
    WeakPtr<BackgroundHTMLParser> m_backgroundParser;
    OwnPtrWillBeMember<HTMLResourcePreloader> m_preloader;

    bool m_shouldUseThreading;
    bool m_endWasDelayed;
    bool m_haveBackgroundParser;
    bool m_tasksWereSuspended;
    unsigned m_pumpSessionNestingLevel;
    unsigned m_pumpSpeculationsSessionNestingLevel;
    bool m_isParsingAtLineNumber;
    RefPtr<EventRacerLog> m_log;
    EventRacerJoinActions m_joinActions;
};

}

#endif
