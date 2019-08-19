#include "JavaScriptFunctionType.h"
#include "Utils.h"

namespace WPEFramework {
namespace JavaScript {
namespace Amazon {

    static char amazonLibrary[] = _T("libamazon_player.so");

    class AmazonPlayer {
    private:
        typedef void ( *MessageListenerType )( const std::string& msg );
        typedef bool ( *RegisterMessageListenerType )( MessageListenerType inMessageListener );
        typedef std::string ( *SendMessageType )( const std::string& );

        static void ListenerCallback (const std::string& msg)
        {
            AmazonPlayer& instance(AmazonPlayer::Instance());

            if (instance._jsContext) {
                //std::cerr << "MESSAGE: " << msg << std::endl;

                JSValueRef passedArgs;

                JSStringRef message = JSStringCreateWithUTF8CString(msg.c_str());
                passedArgs = JSValueMakeString(instance._jsContext, message);

                JSObjectRef globalObject = JSContextGetGlobalObject(instance._jsContext);
                JSObjectCallAsFunction(instance._jsContext, instance._jsListener, globalObject, 1, &passedArgs, NULL);
                JSStringRelease(message);
            }
        }

        AmazonPlayer()
            : library(amazonLibrary)
            , _registerMessageListener(nullptr)
            , _sendMessage(nullptr)
            , _jsContext()
            , _jsListener()
        {
            if (library.IsLoaded() == true) {
                _registerMessageListener = reinterpret_cast<RegisterMessageListenerType>(library.LoadFunction(_T("registerMessageListener")));
                _sendMessage = reinterpret_cast<SendMessageType >(library.LoadFunction(_T("sendMessage")));
            } 

            if ((_registerMessageListener == nullptr) || (_sendMessage == nullptr)) {
                TRACE(Trace::Fatal, (_T("FAILED Library loading: %s"), amazonLibrary));
            }
        }
 
    public:
        AmazonPlayer(const AmazonPlayer&) = delete;
        AmazonPlayer& operator= (const AmazonPlayer&) = delete;

        static AmazonPlayer& Instance() {
            static AmazonPlayer _singleton;
            return (_singleton);
        }

       ~AmazonPlayer() 
        {
            if (_jsListener != nullptr) {
                JSValueUnprotect(_jsContext, _jsListener);
            }
        }

    public:
        void RegisterMessageListener(JSContextRef& context, JSObjectRef& listener) {
            if ( (_jsListener == nullptr) && (_registerMessageListener != nullptr))  {
                assert (_jsContext == nullptr);

                _jsListener = listener;
                JSValueProtect(context, _jsListener);
                _jsContext  = JSContextGetGlobalContext(context);
                _registerMessageListener(ListenerCallback);
            }
        }
        void SendMessage(const string& message) {
            if (_sendMessage != nullptr) {
                _sendMessage(message);
            }
        }

    private:
        Core::Library library;
        RegisterMessageListenerType _registerMessageListener;
        SendMessageType _sendMessage;
        JSContextRef _jsContext;
        JSObjectRef _jsListener;
    };

    class registerMessageListener {
    public:
        registerMessageListener() = default;
        ~registerMessageListener() = default;

        JSValueRef HandleMessage(JSContextRef context, JSObjectRef,
                                 JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*)
        {
            if (argumentCount != 1) {
                TRACE(Trace::Information, (_T("Hawaii::registerMessageListener expects 1 argument")));
            }
            else {
                JSObjectRef jsListener = JSValueToObject(context, arguments[0], nullptr);
        
                if (!JSObjectIsFunction(context, jsListener)) {
                    TRACE(Trace::Information, (_T("Hawaii::registerMessageListener expects a funtion argument")));
                }
                else {
                    AmazonPlayer::Instance().RegisterMessageListener(context, jsListener);
                }
            }

            return JSValueMakeNull(context);
        }
    };

    class sendMessage {
    public:
        sendMessage() = default;
        ~sendMessage() = default;

        JSValueRef HandleMessage(JSContextRef context, JSObjectRef,
                                 JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*)
        {
            if (argumentCount != 1) {
                TRACE(Trace::Information, (_T("Hawaii::sendMessage expects one argument")));
                std::cerr << "sendMessage expects one argument" << std::endl;
            }
            else if (!JSValueIsString(context, arguments[0])) {
                TRACE(Trace::Information, (_T("Hawaii::sendMessage expects a string argument")));
            }
            else {
                JSStringRef jsString = JSValueToStringCopy(context, arguments[0], nullptr);
                size_t bufferSize = JSStringGetLength(jsString) + 1;
                char stringBuffer[bufferSize];

                JSStringGetUTF8CString(jsString, stringBuffer, bufferSize);

                //TRACE(Trace::Information, (_T("Hawaii::sendMessage(%s)"), stringBuffer));

                AmazonPlayer::Instance().SendMessage(stringBuffer);

                JSStringRelease(jsString);
            }

            return JSValueMakeNull(context);
        }
    };

    static JavaScriptFunctionType<registerMessageListener> _registerInstance(_T("hawaii"));
    static JavaScriptFunctionType<sendMessage> _sendInstance(_T("hawaii"));

} // namespace Amazon
} // namespace JavaScript
} // namespace WPEFramework