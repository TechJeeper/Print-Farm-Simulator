module.exports = [
  {
    type: 'heading',
    defaultValue: 'Print Farm Simulator'
  },
  {
    type: 'text',
    defaultValue: 'Tune alerts while your printers earn money on your wrist.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Alerts'
      },
      {
        type: 'toggle',
        messageKey: 'VibeEnabled',
        label: 'Vibrate',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'QuietHours',
        label: 'Quiet hours (10pm–7am)',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'CrisisFrequency',
        label: 'Crisis frequency',
        defaultValue: '1',
        options: [
          { label: 'Low', value: '0' },
          { label: 'Normal', value: '1' },
          { label: 'Chaotic', value: '2' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
