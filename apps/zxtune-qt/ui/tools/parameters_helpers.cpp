/*
Abstract:
  Parameters helpers implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "parameters_helpers.h"
#include "ui/utils.h"
//common includes
#include <contract.h>
#include <logging.h>
#include <tools.h>
//qt includes
#include <QtGui/QAbstractButton>
#include <QtGui/QAction>
#include <QtGui/QComboBox>
#include <QtGui/QGroupBox>
#include <QtGui/QLineEdit>
#include <QtGui/QSlider>
#include <QtGui/QSpinBox>

namespace
{
  using namespace Parameters;

  template<class Holder>
  class BooleanValueImpl : public BooleanValue
  {
  public:
    BooleanValueImpl(Holder& parent, Parameters::Container& ctr, const Parameters::NameType& name, bool defValue, const Parameters::IntType& oneValue)
      : BooleanValue(parent)
      , Parent(parent)
      , Container(ctr)
      , Name(name)
      , Default(defValue)
      , OneValue(oneValue)
    {
      BooleanValueImpl<Holder>::Reload();
      Require(connect(&parent, SIGNAL(toggled(bool)), SLOT(Set(bool))));
    }

    virtual void Set(bool value)
    {
      const Parameters::IntType val = value ? OneValue : 0;
      Log::Debug("Parameters::Helper", "%1%=%2%", Name, val);
      Container.SetValue(Name, val);
    }

    virtual void Reset()
    {
      const AutoBlockSignal block(Parent);
      Container.RemoveValue(Name);
      Reload();
    }

    virtual void Reload()
    {
      Parent.setChecked(GetValue());
    }
  private:
    bool GetValue() const
    {
      Parameters::IntType val = Default ? OneValue : 0;
      Container.FindValue(Name, val);
      return val != 0;
    }
  private:
    Holder& Parent;
    Parameters::Container& Container;
    const Parameters::NameType Name;
    const bool Default;
    const Parameters::IntType OneValue;
  };

  class StringSetValue : public ExclusiveValue
  {
  public:
    StringSetValue(QAbstractButton& parent, Parameters::Container& ctr, const Parameters::NameType& name, const Parameters::StringType& value)
      : ExclusiveValue(parent)
      , Parent(parent)
      , Container(ctr)
      , Name(name)
      , Value(value)
    {
      StringSetValue::Reload();
      Require(connect(&parent, SIGNAL(toggled(bool)), SLOT(Set(bool))));
    }

    virtual void Set(bool value)
    {
      if (value)
      {
        Log::Debug("Parameters::Helper", "%1%=%2%", Name, Value);
        Container.SetValue(Name, Value);
      }
    }

    virtual void Reset()
    {
      const AutoBlockSignal block(Parent);
      Container.RemoveValue(Name);
      Reload();
    }

    virtual void Reload()
    {
      Parent.setChecked(GetValue() == Value);
    }
  private:
    Parameters::StringType GetValue() const
    {
      Parameters::StringType value;
      Container.FindValue(Name, value);
      return value;
    }
  private:
    QAbstractButton& Parent;
    Parameters::Container& Container;
    const Parameters::NameType Name;
    const Parameters::StringType Value;
  };

  template<class Holder>
  void SetWidgetValue(Holder& holder, int val)
  {
    holder.setValue(val);
  }

  template<class Holder>
  void ConnectChanges(Holder& holder, IntegerValue& val)
  {
    Require(val.connect(&holder, SIGNAL(valueChanged(int)), SLOT(Set(int))));
  }

  void SetWidgetValue(QComboBox& holder, int val)
  {
    holder.setCurrentIndex(val);
  }

  void ConnectChanges(QComboBox& holder, IntegerValue& val)
  {
    Require(val.connect(&holder, SIGNAL(currentIndexChanged(int)), SLOT(Set(int))));
  }

  template<class Holder>
  class IntegerValueImpl : public IntegerValue
  {
  public:
    IntegerValueImpl(Holder& parent, Parameters::Container& ctr, const Parameters::NameType& name, int defValue)
      : IntegerValue(parent)
      , Parent(parent)
      , Container(ctr)
      , Name(name)
      , Default(defValue)
    {
      IntegerValueImpl<Holder>::Reload();
      ConnectChanges(Parent, *this);
    }

    virtual void Set(int value)
    {
      Log::Debug("Parameters::Helper", "%1%=%2%", Name, value);
      Container.SetValue(Name, value);
    }

    virtual void Reset()
    {
      const AutoBlockSignal block(Parent);
      Container.RemoveValue(Name);
      Reload();
    }

    virtual void Reload()
    {
      SetWidgetValue(Parent, GetValue());
    }
  private:
    int GetValue() const
    {
      Parameters::IntType value = Default;
      Container.FindValue(Name, value);
      return value;
    }
  private:
    Holder& Parent;
    Parameters::Container& Container;
    const Parameters::NameType Name;
    const int Default;
  };

  class BigIntegerValueImpl : public BigIntegerValue
  {
  public:
    BigIntegerValueImpl(QLineEdit& parent, Parameters::Container& ctr, const IntegerTraits& traits)
      : BigIntegerValue(parent)
      , Parent(parent)
      , Container(ctr)
      , Traits(traits)
    {
      BigIntegerValueImpl::Reload();
      Require(connect(&parent, SIGNAL(textChanged(const QString&)), SLOT(Set(const QString&))));
      Require(connect(&parent, SIGNAL(editingFinished()), SLOT(Reload())));
    }


    virtual void Set(const QString& value)
    {
      const Parameters::IntType val = value.toLongLong();
      if (in_range(val, Traits.Min, Traits.Max))
      {
        Log::Debug("Parameters::Helper", "%1%=%2%", Traits.Name, val);
        Container.SetValue(Traits.Name, val);
      }
    }

    virtual void Reset()
    {
      const AutoBlockSignal block(Parent);
      Container.RemoveValue(Traits.Name);
      Reload();
    }

    virtual void Reload()
    {
      const Parameters::IntType val = GetValue();
      if (in_range(val, Traits.Min, Traits.Max))
      {
        Parent.setText(QString::number(val));
      }
      else
      {
        Parent.clear();
      }
    }
  private:
    Parameters::IntType GetValue() const
    {
      Parameters::IntType value = Traits.Default;
      Container.FindValue(Traits.Name, value);
      return value;
    }
  private:
    QLineEdit& Parent;
    Parameters::Container& Container;
    const Parameters::IntegerTraits Traits;
  };

  class StringValueImpl : public StringValue
  {
  public:
    StringValueImpl(QLineEdit& parent, Parameters::Container& ctr, const Parameters::NameType& name, const Parameters::StringType& defValue)
      : StringValue(parent)
      , Parent(parent)
      , Container(ctr)
      , Name(name)
      , Default(defValue)
    {
      StringValueImpl::Reload();
      Require(connect(&parent, SIGNAL(textChanged(const QString&)), SLOT(Set(const QString&))));
    }

    virtual void Set(const QString& value)
    {
      const Parameters::StringType& val = FromQString(value);
      Log::Debug("Parameters::Helper", "%1%=%2%", Name, val);
      Container.SetValue(Name, val);
    }

    virtual void Reset()
    {
      const AutoBlockSignal block(Parent);
      Container.RemoveValue(Name);
      Reload();
    }

    virtual void Reload()
    {
      Parent.setText(ToQString(GetValue()));
    }
  private:
    Parameters::StringType GetValue() const
    {
      Parameters::StringType value = Default;
      Container.FindValue(Name, value);
      return value;
    }
  private:
    QLineEdit& Parent;
    Parameters::Container& Container;
    const Parameters::NameType Name;
    const Parameters::StringType Default;
  };
}

namespace Parameters
{
  Value::Value(QObject& parent) : QObject(&parent)
  {
  }

  BooleanValue::BooleanValue(QObject& parent) : Value(parent)
  {
  }

  ExclusiveValue::ExclusiveValue(QObject& parent) : Value(parent)
  {
  }

  IntegerValue::IntegerValue(QObject& parent) : Value(parent)
  {
  }

  BigIntegerValue::BigIntegerValue(QObject &parent) : Value(parent)
  {
  }

  StringValue::StringValue(QObject &parent) : Value(parent)
  {
  }

  Value* BooleanValue::Bind(QAction& action, Parameters::Container& ctr, const Parameters::NameType& name, bool defValue)
  {
    return new BooleanValueImpl<QAction>(action, ctr, name, defValue, 1);
  }

  Value* BooleanValue::Bind(QAbstractButton& button, Parameters::Container& ctr, const Parameters::NameType& name, bool defValue, const Parameters::IntType& oneValue)
  {
    return new BooleanValueImpl<QAbstractButton>(button, ctr, name, defValue, oneValue);
  }

  Value* BooleanValue::Bind(QGroupBox& box, Parameters::Container& ctr, const Parameters::NameType& name, bool defValue, const Parameters::IntType& oneValue)
  {
    return new BooleanValueImpl<QGroupBox>(box, ctr, name, defValue, oneValue);
  }

  Value* ExclusiveValue::Bind(QAbstractButton& button, Parameters::Container& ctr, const Parameters::NameType& name, const Parameters::StringType& value)
  {
    return new StringSetValue(button, ctr, name, value);
  }

  Value* IntegerValue::Bind(QComboBox& combo, Parameters::Container& ctr, const Parameters::NameType& name, int defValue)
  {
    return new IntegerValueImpl<QComboBox>(combo, ctr, name, defValue);
  }

  Value* IntegerValue::Bind(QSlider& slider, Parameters::Container& ctr, const Parameters::NameType& name, int defValue)
  {
    return new IntegerValueImpl<QSlider>(slider, ctr, name, defValue);
  }

  Value* IntegerValue::Bind(QSpinBox& spinbox, Parameters::Container& ctr, const Parameters::NameType& name, int defValue)
  {
    return new IntegerValueImpl<QSpinBox>(spinbox, ctr, name, defValue);
  }

  Value* BigIntegerValue::Bind(QLineEdit& edit, Parameters::Container& ctr, const IntegerTraits& traits)
  {
    return new BigIntegerValueImpl(edit, ctr, traits);
  }

  Value* StringValue::Bind(QLineEdit& edit, Parameters::Container& ctr, const Parameters::NameType& name, const Parameters::StringType& defValue)
  {
    return new StringValueImpl(edit, ctr, name, defValue);
  }
}
