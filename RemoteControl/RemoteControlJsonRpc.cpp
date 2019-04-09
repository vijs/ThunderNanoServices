
#include <interfaces/json/JsonData_RemoteControl.h>
#include "RemoteControl.h"
#include "Module.h"

namespace WPEFramework {

namespace Plugin {

    using namespace JsonData::RemoteControl;

    // Registration
    //

    void RemoteControl::RegisterAll()
    {
        Register<void,Core::JSON::ArrayType<Core::JSON::String>>(_T("devices"), &RemoteControl::endpoint_devices, this);
        Register<DeviceParamsInfo,DeviceResultData>(_T("device"), &RemoteControl::endpoint_device, this);
        Register<KeyParamsInfo,KeyResultData>(_T("key"), &RemoteControl::endpoint_key, this);
        Register<KeyParamsInfo,void>(_T("delete"), &RemoteControl::endpoint_delete, this);
        Register<RcinfoInfo,void>(_T("modify"), &RemoteControl::endpoint_modify, this);
        Register<DeviceParamsInfo,void>(_T("pair"), &RemoteControl::endpoint_pair, this);
        Register<UnpairParamsData,void>(_T("unpair"), &RemoteControl::endpoint_unpair, this);
        Register<RcinfoInfo,void>(_T("send"), &RemoteControl::endpoint_send, this);
        Register<RcinfoInfo,void>(_T("press"), &RemoteControl::endpoint_press, this);
        Register<RcinfoInfo,void>(_T("release"), &RemoteControl::endpoint_release, this);
        Register<DeviceParamsInfo,void>(_T("save"), &RemoteControl::endpoint_save, this);
        Register<DeviceParamsInfo,void>(_T("load"), &RemoteControl::endpoint_load, this);
        Register<RcinfoInfo,void>(_T("add"), &RemoteControl::endpoint_add, this);
    }

    void RemoteControl::UnregisterAll()
    {
        Unregister(_T("add"));
        Unregister(_T("load"));
        Unregister(_T("save"));
        Unregister(_T("release"));
        Unregister(_T("press"));
        Unregister(_T("send"));
        Unregister(_T("unpair"));
        Unregister(_T("pair"));
        Unregister(_T("modify"));
        Unregister(_T("delete"));
        Unregister(_T("key"));
        Unregister(_T("device"));
        Unregister(_T("devices"));
    }

   // Helpers
   //

   Core::JSON::ArrayType<Core::JSON::EnumType<ModifiersType>> RemoteControl::Modifiers(uint16_t modifiers) const
   {
       Core::JSON::ArrayType<Core::JSON::EnumType<ModifiersType>> response;

       uint16_t flag(1);
       while (modifiers != 0) {

           if ((modifiers & 0x01) != 0) {
               switch (flag) {
               case ModifiersType::LEFTSHIFT:
               case ModifiersType::RIGHTSHIFT:
               case ModifiersType::LEFTALT:
               case ModifiersType::RIGHTALT:
               case ModifiersType::LEFTCTRL:
               case ModifiersType::RIGHTCTRL: {
                   Core::JSON::EnumType<ModifiersType>& jsonRef = response.Add();
                   jsonRef = static_cast<ModifiersType>(flag);
                   break;
               }
               default:
                   ASSERT(false);
                   break;
               }
           }

           flag = flag << 1;
           modifiers = modifiers >> 1;
       }
       return (response);
   }

   uint16_t RemoteControl::Modifiers(const Core::JSON::ArrayType<Core::JSON::EnumType<ModifiersType>>& param) const
   {
       uint16_t modifiers = 0;

       if (param.IsSet()) {

           Core::JSON::ArrayType<Core::JSON::EnumType<ModifiersType>>::ConstIterator flags(param.Elements());

           while (flags.Next() == true) {
               switch (flags.Current().Value()) {
               case ModifiersType::LEFTSHIFT:
               case ModifiersType::RIGHTSHIFT:
               case ModifiersType::LEFTALT:
               case ModifiersType::RIGHTALT:
               case ModifiersType::LEFTCTRL:
               case ModifiersType::RIGHTCTRL:
                   modifiers |= flags.Current().Value();
                   break;
               default:
                   ASSERT(false);
                   break;
               }
           }
       }
       return (modifiers);
   }

    // API implementation
    //

    // General information.
    uint32_t RemoteControl::endpoint_devices(Core::JSON::ArrayType<Core::JSON::String>& response)
    {
        // Add virtual devices
        std::list<string>::const_iterator index(_virtualDevices.begin());

        while (index != _virtualDevices.end()) {
            Core::JSON::String newElement;
            newElement = *index;
            response.Add(newElement);
            index++;
        }

        // Look at specific devices, if we have, append them to response
        Remotes::RemoteAdministrator& admin(Remotes::RemoteAdministrator::Instance());
        Remotes::RemoteAdministrator::Iterator remoteDevices(admin.Producers());

        while (remoteDevices.Next() == true) {
            Core::JSON::String newElement;
            newElement = (*remoteDevices)->Name();
            response.Add(newElement);
        }
        return (Core::ERROR_NONE);
    }

    // Get device.
    uint32_t RemoteControl::endpoint_device(const DeviceParamsInfo& params, DeviceResultData& response)
    {
        uint32_t result = Core::ERROR_NONE;

        if(params.Device.IsSet() == true) {
            if (IsVirtualDevice(params.Device.Value()) == true) {
                result = Core::ERROR_GENERAL;
            } else if (IsPhysicalDevice(params.Device.Value()) == true) {
                Remotes::RemoteAdministrator& admin(Remotes::RemoteAdministrator::Instance());
                uint32_t error = admin.Error(params.Device.Value());

                if (error == Core::ERROR_NONE) {
                    // Look at specific devices, if we have, append them to response
                    Remotes::RemoteAdministrator::Iterator remoteDevices(admin.Producers());

                    while (remoteDevices.Next() == true) {
                        if ((*remoteDevices)->Name() == params.Device.Value()) {
                            response.Name = (*remoteDevices)->Name();
                            response.Metadata = (*remoteDevices)->MetaData();
                            break;
                        }
                    }
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key action.
    uint32_t RemoteControl::endpoint_key(const KeyParamsInfo& params, KeyResultData& response)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0)) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                // Load default or specific device mapping
                PluginHost::VirtualInput::KeyMap& map(_inputHandler->Table(params.Device.Value()));
                const PluginHost::VirtualInput::KeyMap::ConversionInfo* codeElements = map[params.Code.Value()];

                if (codeElements != nullptr) {
                    response.Code = params.Code.Value();
                    response.Key = codeElements->Code;
                    response.Modifiers = Modifiers(codeElements->Modifiers);
                } else {
                    result = Core::ERROR_UNKNOWN_KEY;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_delete(const KeyParamsInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0)) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                // Load default or specific device mapping
                PluginHost::VirtualInput::KeyMap& map(_inputHandler->Table(params.Device.Value()));
                const PluginHost::VirtualInput::KeyMap::ConversionInfo* codeElements = map[params.Code.Value()];
                if (codeElements != nullptr) {
                    map.Delete(params.Code.Value());
                } else {
                    result = Core::ERROR_UNKNOWN_KEY;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_modify(const RcinfoInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0) && (params.Key.IsSet()) && (params.Modifiers.IsSet())) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                // Load default or specific device mapping
                PluginHost::VirtualInput::KeyMap& map(_inputHandler->Table(params.Device.Value()));
                if (map.Modify(params.Code.Value(), params.Key.Value(), Modifiers(params.Modifiers)) == false) {
                    result = Core::ERROR_UNKNOWN_KEY;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Pairing device.
    uint32_t RemoteControl::endpoint_pair(const DeviceParamsInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if (params.Device.IsSet() == true) {
            if (IsPhysicalDevice(params.Device.Value()) == true) {
                if (Remotes::RemoteAdministrator::Instance().Pair(params.Device.Value()) == false) {
                    result = Core::ERROR_GENERAL;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Pairing device.
    uint32_t RemoteControl::endpoint_unpair(const UnpairParamsData& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Bindid.IsSet())) {
            if (IsPhysicalDevice(params.Device.Value()) == true) {
                if (Remotes::RemoteAdministrator::Instance().Unpair(params.Device.Value(), params.Bindid.Value()) == false) {
                    result = Core::ERROR_GENERAL;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_send(const RcinfoInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0) && (params.Key.IsSet()) && (params.Modifiers.IsSet())) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                result = KeyEvent(true, params.Code.Value(), params.Device.Value());
                if (result == Core::ERROR_NONE) {
                    result = KeyEvent(false, params.Code.Value(), params.Device.Value());
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_press(const RcinfoInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0) && (params.Key.IsSet()) && (params.Modifiers.IsSet())) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                result = KeyEvent(true, params.Code.Value(), params.Device.Value());
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_release(const RcinfoInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0) && (params.Key.IsSet()) && (params.Modifiers.IsSet())) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                result = KeyEvent(false, params.Code.Value(), params.Device.Value());
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_save(const DeviceParamsInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if (params.Device.IsSet() == true) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                string fileName;

                if (_persistentPath.empty() == false) {
                    Core::Directory directory(_persistentPath.c_str());
                    if (directory.CreatePath()) {
                        fileName = _persistentPath + params.Device.Value() + _T(".json");
                    }
                }

                if (fileName.empty() == false) {
                    // Seems like we have a default mapping file. Load it..
                    PluginHost::VirtualInput::KeyMap& map(_inputHandler->Table(params.Device.Value()));
                    result = map.Save(fileName);
                } else {
                    result = Core::ERROR_GENERAL;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_load(const DeviceParamsInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;

        if (params.Device.IsSet() == true) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                string fileName = _persistentPath + params.Device.Value() + _T(".json");

                if (Core::File(fileName).Exists() == true) {
                    // Seems like we have a default mapping file. Load it..
                    PluginHost::VirtualInput::KeyMap& map(_inputHandler->Table(params.Device.Value()));
                    result = map.Load(fileName);
                } else {
                    result = Core::ERROR_OPENING_FAILED;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

    // Key mapping actions.
    uint32_t RemoteControl::endpoint_add(const RcinfoInfo& params)
    {
        uint32_t result = Core::ERROR_NONE;
        if ((params.Device.IsSet() == true) && (params.Code.IsSet() == true) && (params.Code.Value() != 0) && (params.Key.IsSet()) && (params.Modifiers.IsSet())) {
            if ((IsVirtualDevice(params.Device.Value()) == true) || (IsPhysicalDevice(params.Device.Value()) == true)) {
                // Load default or specific device mapping
                PluginHost::VirtualInput::KeyMap& map(_inputHandler->Table(params.Device.Value()));
                if (map.Add(params.Code.Value(), params.Key.Value(), Modifiers(params.Modifiers)) == false) {
                    result = Core::ERROR_UNKNOWN_KEY;
                }
            } else {
                result = Core::ERROR_UNAVAILABLE;
            }
        } else {
            result = Core::ERROR_BAD_REQUEST;
        }
        return result;
    }

} // namespace Plugin

} // namespace WPEFramework
